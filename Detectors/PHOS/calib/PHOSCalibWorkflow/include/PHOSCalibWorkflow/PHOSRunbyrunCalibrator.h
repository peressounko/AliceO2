// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef O2_CALIBRATION_PHOSRUNBYRUN_CALIBRATOR_H
#define O2_CALIBRATION_PHOSRUNBYRUN_CALIBRATOR_H

/// @file   PHOSRunbyrunCalibDevice.h
/// @brief  Device to calculate PHOS energy run by run corrections

#include "Framework/Task.h"
#include "Framework/ProcessingContext.h"
#include "PHOSCalib/BadChannelMap.h"
#include "DetectorsCalibration/TimeSlotCalibration.h"
#include <boost/histogram.hpp>
#include "TH1.h"
#include "PHOSCalibWorkflow/RingBuffer.h"
#include "PHOSReconstruction/FullCluster.h"
#include "DataFormatsPHOS/TriggerRecord.h"
#include "PHOSBase/Geometry.h"

using namespace o2::framework;

namespace o2
{
namespace phos
{

class PHOSRunbyrunSlot
{
 public:
  using boostHisto = boost::histogram::histogram<std::tuple<boost::histogram::axis::regular<double, boost::use_default, boost::use_default, boost::use_default>>, boost::histogram::unlimited_storage<std::allocator<char>>>;

  PHOSRunbyrunSlot(bool useCCDB, std::string path);
  PHOSRunbyrunSlot(const PHOSRunbyrunSlot& other);

  ~PHOSRunbyrunSlot() = default;

  void print() const;
  void fill(const std::vector<FullCluster>& clusters, const gsl::span<const TriggerRecord>& trs);
  void fill(const gsl::span<const FullCluster>& /*clusters*/){}; //not used
  void merge(const PHOSRunbyrunSlot* prev);
  void clear();

  boostHisto& getCollectedHistos(int m) { return mReMi[m]; }

  void setRunStartTime(long tf) { mRunStartTime = tf; }

 private:
  bool checkCluster(const FullCluster& clu);

 private:
  bool mUseCCDB = false;
  long mRunStartTime = 0;                                 /// start time of the run (sec)
  float mPtCut = 1.5;                                     /// ptmin of a pair cut (GeV/c)
  std::string mCCDBPath{"http://ccdb-test.cern.ch:8080"}; /// CCDB path to retrieve current CCDB objects for comparison
  std::array<boostHisto, 8> mReMi;                        /// Real and Mixed inv mass distributions per module
  std::unique_ptr<RingBuffer> mBuffer;                    /// Buffer for current and previous events
  std::unique_ptr<BadChannelMap> mBadMap;                 /// Latest bad channels map

  ClassDefNV(PHOSRunbyrunSlot, 1);
};

//==========================================================================================
class PHOSRunbyrunCalibrator final : public o2::calibration::TimeSlotCalibration<o2::phos::FullCluster, o2::phos::PHOSRunbyrunSlot>
{
  using Slot = o2::calibration::TimeSlot<o2::phos::PHOSRunbyrunSlot>;

 public:
  PHOSRunbyrunCalibrator();
  ~PHOSRunbyrunCalibrator() final ;

  bool hasEnoughData(const Slot& slot) const final;
  void initOutput() final;
  void finalizeSlot(Slot& slot) final;
  Slot& emplaceNewSlot(bool front, uint64_t tstart, uint64_t tend) final;
  bool process(uint64_t tf, const std::vector<FullCluster>& clu, const gsl::span<const TriggerRecord>& trs);

  std::array<float, 8> getCalibration() { return mRunByRun; }
  void endOfStream();

  //Functions used in histo fittings
  double CBRatio(double* x, double* p);
  double CBSignal(double* x, double* p);
  double bg(double* x, double* p);

 private:
  void scanClusters(o2::framework::ProcessingContext& pc);
  bool checkCluster(const FullCluster& clu);

 private:
  bool mUseCCDB = false;
  long mRunStartTime = 0;                                 /// start time of the run (sec)
  std::string mCCDBPath{"http://ccdb-test.cern.ch:8080"}; /// CCDB path to retrieve current CCDB objects for comparison
  std::array<float, 8> mRunByRun;                         /// Final calibration object
  std::array<TH1F*, 8> mReMi;                             /// Real and Mixed inv mass distributions per module

  ClassDefOverride(PHOSRunbyrunCalibrator, 1);
};

} // namespace phos
} // namespace o2

#endif
