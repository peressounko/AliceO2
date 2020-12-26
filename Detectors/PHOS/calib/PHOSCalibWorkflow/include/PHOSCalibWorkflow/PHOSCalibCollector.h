// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef O2_PHOS_CALIB_COLLECTOR_H
#define O2_PHOS_CALIB_COLLECTOR_H

/// @file   PHOSCalibCollectorSpec.h
/// @brief  Device to collect information for PHOS energy and time calibration.

#include "DetectorsCalibration/Utils.h"
#include "DataFormatsTOF/CalibInfoTOF.h"
#include "Framework/Task.h"
#include "Framework/ConfigParamRegistry.h"
#include "Framework/ControlService.h"
#include "Framework/WorkflowSpec.h"

using namespace o2::framework;

namespace o2
{
namespace phos
{

 union CalibDigit{
    uint32_t mDataWord;
    struct {
      uint32_t mAddress : 14 ;   ///< Bits  0 - 13: Hardware address
      uint32_t mAdcAmp  : 10 ;   ///< Bits 14 - 23: ADC counts
      uint32_t mHgLg    : 1 ;    ///< Bit  24: LG/HG
      uint32_t mBadChannel :1 ;  ///< Bit  25: Bad channel status
      uint32_t mCluster : 6;     ///< Bits 26-32: index of cluster in event
    };
 } ;

 union EventHeader{
    uint32_t mDataWord;
    struct {
      uint32_t mMarker  : 14 ;    ///< Bits  0 - 13: non-existing address to separate events 16383
      uint32_t mVtxBin  : 4 ;     ///< Bits 14-18: z position of vertex      
      uint32_t mEventId : 14 ;    ///< Bits 14 - 23: ADC counts
    };
 } ;

// For real/mixed distribution calculation
class RungBuffer
{
  public:
    RungBuffer() = default ;
    ~RungBuffer() = default ;

  short size(){return short(mBuffer.size);}  
  short addEntry(TLorentzVector v){
    if(mBuffer.size()<kBufferSize){
      mBuffer.emplace_back(&v);mCurrent++; mCurrent=mCurrent%kBufferSize ; return 1; }
    else{mBuffer[mCurrent++]=v; mCurrent=mCurrent%kBufferSize ; return 0;}
  } 
  const TLorentzVector getEntry(short index){
    short indx=(mCurrent-mBuffer.size()+index)%kBufferSize ;
    if(indx<0) printf("ERROR index=%d, indx=%d \n",index,indx) ;
    return mBuffer[indx] ;
  }                               

  private:
  static constexpr short kBufferSize = 100;             ///< Total size of the buffer
  std::vector<TLorentzVector> mBuffer(kBufferSize);     ///< buffer

}


class PHOSCalibCollector : public o2::framework::Task
{

  enum hnames{kReInvMassPerCell,kMiInvMassPerCell,kReInvMassNonlin,kMiInvMassNonlin,kTimeHGPerCell,kTimeLGPerCell,kTimeHGSlewing,kTimeLGSlewing } ;

 public:
  void init(o2::framework::InitContext& ic) final;

  void run(o2::framework::ProcessingContext& pc) final ;

  void endOfStream(o2::framework::EndOfStreamContext& ec) final ;

 private:
  int mMixed = 5 ;                                   // Number of events used for mixed events
  std::vector<uint32_t> mDigits;
  std::vector<TH2F> mHistos ;
  RungBuffer mEventBuffer;                           // Buffer for current and previous events
  uint32_t mEvent = 0;
  ClassDefNV(PHOSEnergyCalibCollector, 1);

 };

o2::framework::DataProcessorSpec getPHOSCalibCollectorDeviceSpec() ;

} // namespace phos
} // namespace o2

#endif
