// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef O2_CALIBRATION_PHOSCALIB_COLLECTOR_WRITER_H
#define O2_CALIBRATION_PHOSCALIB_COLLECTOR_WRITER_H

/// @file   PHOSCalibCollectorWriterSpec.h
/// @brief  Device to write to tree the information for PHOS energy-time calibration.

#include <TTree.h>

using namespace o2::framework;

namespace o2
{
namespace phos
{

class PHOSCalibCollectorWriter : public o2::framework::Task
{

 public:
  void createAndOpenFileAndTree() ;

  void init(o2::framework::InitContext& ic) final ;

  void run(o2::framework::ProcessingContext& pc) final ;

  void endOfStream(o2::framework::EndOfStreamContext& ec) final ;

 private:
  void sendOutput(DataAllocator& output) ;

  int mChank = 0;                            // Number of chanks of digits wrote to file
  bool mIsEndOfStream = false;
  std::unique_ptr<TFile> mfileOutCells ;     // file to write calibration cells
  std::unique_ptr<TFile> mFileTimeHisto ;    // file to write time calibration histograms
  std::unique_ptr<TFile> mFileEnergyHisto ;  // file to write energy calibration histograms

};
} // namespace phos

namespace framework
{

DataProcessorSpec getPHOSCalibCollectorWriterSpec() ;

} // namespace framework
} // namespace o2

#endif
