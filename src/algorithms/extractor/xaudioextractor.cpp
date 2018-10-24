/*
 * Copyright (C) 2006-2016  Music Technology Group - Universitat Pompeu Fabra
 *
 * This file is part of Essentia
 *
 * Essentia is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation (FSF), either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the Affero GNU General Public License
 * version 3 along with this program.  If not, see http://www.gnu.org/licenses/
 */

#include "xaudioextractor.h"
#include "algorithmfactory.h"
#include "essentiamath.h"
#include "poolstorage.h"
#include "copy.h"

//#define USE_RING_BUFFER

using namespace std;
using namespace essentia;
using namespace essentia::streaming;

const char* XAudioExtractor::name = "XAudioExtractor";
const char* XAudioExtractor::category = "Extractors";
const char* XAudioExtractor::description = DOC("This algorithm extracts all low-level spectral features, which do not require an equal-loudness filter for their computation, from an audio signal");

XAudioExtractor::XAudioExtractor() : _configured(false) {
  // input:
#ifndef USE_RING_BUFFER
  declareInput(_signal, "signal", "the input audio signal");
#endif

  // outputs:
  declareOutput(_mfccs, "mfcc", "See MFCC algorithm documentation");
  declareOutput(_rmsValue, "spectral_rms", "See RMS algorithm documentation");
  declareOutput(_rolloffValue, "spectral_rolloff", "See RollOff algorithm documentation");
  declareOutput(_zeroCrossingRate, "zerocrossingrate", "See ZeroCrossingRate algorithm documentation");

  // create network (instantiate algorithms)
  createInnerNetwork();
#ifndef USE_RING_BUFFER
  _signal                              >>  _frameCutter->input("signal");
#endif
  _frameCutter->output("frame")        >>  _zcr->input("signal");
  _zcr->output("zeroCrossingRate")     >>  _zeroCrossingRate;
  _frameCutter->output("frame")        >>  _windowing->input("frame");
  _windowing->output("frame")          >>  _spectrum->input("frame");
  _spectrum->output("spectrum")        >>  _mfcc->input("spectrum");
  _mfcc->output("mfcc")                >>  _mfccs;
  _mfcc->output("bands")               >>  NOWHERE;
  _spectrum->output("spectrum")        >>  _rms->input("array");
  _rms->output("rms")                  >>  _rmsValue;
  _spectrum->output("spectrum")        >>  _rollOff->input("spectrum");
  _rollOff->output("rollOff")          >>  _rolloffValue;
  _network = new scheduler::Network(_frameCutter);
}

void XAudioExtractor::createInnerNetwork() {
  AlgorithmFactory& factory = AlgorithmFactory::instance();

  _frameCutter        = factory.create("FrameCutter");
  _mfcc               = factory.create("MFCC");
  _rms                = factory.create("RMS");
  _rollOff            = factory.create("RollOff");
  _spectrum           = factory.create("Spectrum");
  _windowing          = factory.create("Windowing",
                                       "type", "blackmanharris62");
  _zcr                = factory.create("ZeroCrossingRate");
}

void XAudioExtractor::configure() {
  int frameSize   = parameter("frameSize").toInt();
  int hopSize     = parameter("hopSize").toInt();
  Real sampleRate = parameter("sampleRate").toReal();
  _frameCutter->configure("silentFrames", "noise", "hopSize", hopSize, "frameSize", frameSize);
}

XAudioExtractor::~XAudioExtractor() {
  clearAlgos();
}

void XAudioExtractor::clearAlgos() {
  if (!_configured) return;
  delete _network;
}

namespace essentia {
namespace standard {

const char* XAudioExtractor::name = essentia::streaming::XAudioExtractor::name;
const char* XAudioExtractor::category = essentia::streaming::XAudioExtractor::category;
const char* XAudioExtractor::description = essentia::streaming::XAudioExtractor::description;

XAudioExtractor::XAudioExtractor() :_ringBufferInput(0) {
  declareInput(_signal, "signal", "the audio input signal");
  declareOutput(_mfcc, "mfcc", "See MFCC algorithm documentation");
  declareOutput(_rms, "spectral_rms", "See RMS algorithm documentation");
  declareOutput(_rollOff, "spectral_rolloff", "See RollOff algorithm documentation");
  declareOutput(_zeroCrossingRate, "zerocrossingrate", "See ZeroCrossingRate algorithm documentation");

  _lowLevelExtractor = streaming::AlgorithmFactory::create("XAudioExtractor");
#ifdef USE_RING_BUFFER
  _ringBufferInput = new RingBufferInput();
//  ParameterMap pmap;
//  pmap.add("bufferSize", 1024);
//  _ringBufferInput->setParameters(pmap);
  _ringBufferInput->configure();
#else
  _vectorInput = new streaming::VectorInput<Real>();
#endif

  createInnerNetwork();
}


XAudioExtractor::~XAudioExtractor() {
  delete _network;
}

void XAudioExtractor::reset() {
  _network->reset();
  _pool.remove("mfcc");
  _pool.remove("rms");
  _pool.remove("rollOff");
  _pool.remove("zeroCrossingRate");    
}

void XAudioExtractor::configure() {
  _lowLevelExtractor->configure(INHERIT("frameSize"), INHERIT("hopSize"), INHERIT("sampleRate"));
}

void XAudioExtractor::createInnerNetwork() {
#ifdef USE_RING_BUFFER
  _ringBufferInput->output("signal") >> ((streaming::XAudioExtractor*)_lowLevelExtractor)->_frameCutter->input("signal");
#else
  *_vectorInput >> _lowLevelExtractor->input("signal");
#endif
  _lowLevelExtractor->output("mfcc") >> PC(_pool, "mfcc");
  _lowLevelExtractor->output("spectral_rms") >> PC(_pool, "rms");
  _lowLevelExtractor->output("spectral_rolloff") >> PC(_pool, "rollOff");
  _lowLevelExtractor->output("zerocrossingrate") >> PC(_pool, "zeroCrossingRate");

  _network = new scheduler::Network(
#ifdef USE_RING_BUFFER
  _ringBufferInput
#else
  _vectorInput
#endif
  );
}

void XAudioExtractor::compute() {
    const vector<Real>& signal = _signal.get();

#ifdef USE_RING_BUFFER
    _ringBufferInput->add((Real*)&signal[0], signal.size());
    //_ringBufferInput->process();
#else
    _vectorInput->setVector(&signal);
#endif

  _network->run();

  vector<vector<Real> > & mfcc = _mfcc.get();
  vector<Real> & rms = _rms.get();
  vector<Real> & rollOff = _rollOff.get();
  vector<Real> & zeroCrossingRate = _zeroCrossingRate.get();

  mfcc =               _pool.value<vector<vector<Real> > >("mfcc");
  rms =                _pool.value<vector<Real> >("rms");
  rollOff =            _pool.value<vector<Real> >("rollOff");
  zeroCrossingRate =   _pool.value<vector<Real> >("zeroCrossingRate");

  reset();
}

} // namespace standard
} // namespace essentia
