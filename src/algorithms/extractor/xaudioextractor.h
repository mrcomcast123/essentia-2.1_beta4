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

#ifndef XAUDIO_EXTRACTOR_H
#define XAUDIO_EXTRACTOR_H

#include "streamingalgorithmcomposite.h"
#include "pool.h"
#include "algorithm.h"
#include "vectorinput.h"
#include "network.h"
#include "ringbufferinput.h"

namespace essentia {
namespace streaming {

class XAudioExtractor : public AlgorithmComposite {
 public:
  SinkProxy<Real> _signal;

  SourceProxy<std::vector<Real> > _mfccs;
  SourceProxy<Real> _rmsValue;
  SourceProxy<Real> _rolloffValue;
  SourceProxy<Real> _zeroCrossingRate;

  Algorithm *_frameCutter,
            *_mfcc,
            *_rms, 
            *_rollOff,
            *_spectrum,
            *_windowing,
             *_zcr;

  scheduler::Network* _network;

  bool _configured;
  void clearAlgos();

 public:
  XAudioExtractor();
  ~XAudioExtractor();

  void declareParameters() {
    declareParameter("frameSize", "the frame size for computing low level features", "(0,inf)", 2048);
    declareParameter("hopSize", "the hop size for computing low level features", "(0,inf)", 1024);
    declareParameter("sampleRate", "the audio sampling rate", "(0,inf)", 44100.0);
  }

  void declareProcessOrder() {
    declareProcessStep(ChainFrom(_frameCutter));
  }

  void configure();
  void createInnerNetwork();

  static const char* name;
  static const char* category;
  static const char* description;
};

} // namespace streaming
} // namespace essentia

namespace essentia {
namespace standard {

class XAudioExtractor : public Algorithm {
 protected:
  Input<std::vector<Real> > _signal;
  Output<std::vector<std::vector<Real> > > _mfcc;
  Output<std::vector<Real> > _rms;
  Output<std::vector<Real> > _rollOff;
  Output<std::vector<Real> > _zeroCrossingRate;

  bool _configured;

  streaming::Algorithm* _lowLevelExtractor;
  streaming::VectorInput<Real>* _vectorInput;
  streaming::RingBufferInput * _ringBufferInput;
  scheduler::Network* _network;
  Pool _pool;

 public:

  XAudioExtractor();
  ~XAudioExtractor();

  void declareParameters() {
    declareParameter("frameSize", "the frame size for computing low level features", "(0,inf)", 2048);
    declareParameter("hopSize", "the hop size for computing low level features", "(0,inf)", 1024);
    declareParameter("sampleRate", "the audio sampling rate", "(0,inf)", 44100.0);
  }

  void configure();
  void createInnerNetwork();
  void compute();
  void reset();

  static const char* name;
  static const char* category;
  static const char* description;
};

}
}

#endif
