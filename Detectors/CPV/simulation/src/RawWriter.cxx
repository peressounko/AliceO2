// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "FairLogger.h"

#include <fmt/core.h>
#include <gsl/span>
#include <TSystem.h>
#include "DataFormatsCPV/RawFormats.h"
#include "CPVSimulation/RawWriter.h"
#include "CPVBase/CPVSimParams.h"
#include "CPVBase/RCUTrailer.h"
#include "CPVBase/Geometry.h"
#include "CCDB/CcdbApi.h"

using namespace o2::cpv;

void RawWriter::init()
{
  mRawWriter = std::make_unique<o2::raw::RawFileWriter>(o2::header::gDataOriginCPV, false);
  // mRawWriter->setCarryOverCallBack(this);
  mRawWriter->setApplyCarryOverToLastPage(false);

  // initialize containers for SRU
  for (auto isru = 0; isru < o2::cpv::Geometry::kNDDL; isru++) {
    SRUDigitContainer srucont;
    srucont.mSRUid = isru;
    mSRUdata.push_back(srucont);
  }
    
  // Set output file and register link
  std::string rawfilename = mOutputLocation;
  rawfilename += "/cpv.raw";
  
  for(int ddl=0; ddl<kNDDL; ddl++){
    short crorc=0, link=ddl;
    mRawWriter->registerLink(ddl, crorc, link, 0, rawfilename.data());
  }


}

void RawWriter::digitsToRaw(gsl::span<o2::cpv::Digit> digitsbranch, gsl::span<o2::cpv::TriggerRecord> triggerbranch)
{
  if (!mCalibParams) {
    if (o2::cpv::CPVSimParams::Instance().mCCDBPath.compare("localtest") == 0) {
      mCalibParams = new CalibParams(1); // test default calibration
      LOG(INFO) << "[RawWriter] No reading calibration from ccdb requested, set default";
    } else {
      LOG(INFO) << "[RawWriter] getting calibration object from ccdb";
      o2::ccdb::CcdbApi ccdb;
      std::map<std::string, std::string> metadata;
      ccdb.init("http://ccdb-test.cern.ch:8080"); // or http://localhost:8080 for a local installation
      auto tr = triggerbranch.begin();
      double eventTime = -1;
      // if(tr!=triggerbranch.end()){
      //   eventTime = (*tr).getBCData().getTimeNS() ;
      // }
      mCalibParams = ccdb.retrieveFromTFileAny<o2::cpv::CalibParams>("CPV/Calib", metadata, eventTime);
      if (!mCalibParams) {
        LOG(FATAL) << "[RawWriter] can not get calibration object from ccdb";
      }
    }
  }

  for (auto trg : triggerbranch) {
    processTrigger(digitsbranch, trg);
  }
}

bool RawWriter::processTrigger(const gsl::span<o2::cpv::Digit> digitsbranch, const o2::cpv::TriggerRecord& trg)
{
  auto srucont = mSRUdata.begin();
  while (srucont != mSRUdata.end()) {
    srucont->mChannels.clear();
    srucont++;
  }
  std::vector<o2::cpv::Digit*>* digitsList;
  for (auto& dig : gsl::span(digitsbranch.data() + trg.getFirstEntry(), trg.getNumberOfObjects())) {
    short absId = dig.getAbsId();
    short ddl, dilogic, row,hwAddr;
    o2::cpv::Geometry::absIdToHWaddress(absId, ddl, dilogic, row, hwAddr ) ;

    //Collect possible several digits (signal+pileup) into one map record
    auto celldata = mSRUdata[ddl].mChannels.find(absId);
    if (celldata == mSRUdata[ddl].mChannels.end()) {
      const auto it = mSRUdata[ddl].mChannels.insert(celldata, {absId, std::vector<o2::cpv::Digit*>()});
      it->second.push_back(&dig);
    } else {
      celldata->second.push_back(&dig);
    }
  }

  // Create and fill DMA pages for each channel

  for(int i=kNDDL; --i; ){    
    for(int j=kNRow; --j; ){
      for(int k=kNDilogic; --k; ){
        mPadCharge[i][j][k].clear();
      }
    }
  }


  //To sort digits in proper order fill array
  for (srucont = mSRUdata.begin(); srucont != mSRUdata.end(); srucont++) {
    short ddl = srucont->mSRUid;

    for (auto ch = srucont->mChannels.cbegin(); ch != srucont->mChannels.cend(); ch++) {
      for (auto dig : ch->second) {
        //Convert energy and time to ADC counts and time ticks
        short charge = dig->getAmplitude() / mCalibParams->getGain(dig->getAbsId());
        if(charge>2047)charge=2047;
        short ddl, dilogic, row,hw;
        o2::cpv::Geometry::absIdToHWaddress(dig->getAbsId(), ddl, dilogic, row, hw ) ;
printf("absId=%d, [ddl=%d,row=%d,dil=%d] hw=%d \n",dig->getAbsId(), ddl, dilogic, row, hw ) ;
        mPadCharge[ddl][row][dilogic].emplace_back(charge,hw) ;
      }
    }
  }

  //Do through DLLs and fill raw words in proper order
  for(int ddl=0; ddl<kNDDL; ddl++){
    mPayload.clear();
    //write empty header, later will be updated ?

    int nwInSegment=0;
    int posRowMarker=0;
    for(int row=0; row<kNRow; row++){
      //reserve place for row header
      mPayload.emplace_back(uint32_t(0)) ;
      posRowMarker= mPayload.size()-1;
      nwInSegment++ ;
      int nwRow=0;
      for(Int_t dilogic = 0; dilogic < kNDilogic; dilogic++){
        int nPad = 0;
if(mPadCharge[ddl][row][dilogic].size()>48){
for(padCharge & pc: mPadCharge[ddl][row][dilogic]){
printf("list: [ddl=%d,row=%d,dil=%d] hw=%d \n",ddl,row,dilogic,pc.pad);
}
printf("===========\n") ;
}        
        for(padCharge & pc: mPadCharge[ddl][row][dilogic]){
          PadWord currentword = {0};          
          currentword.charge = pc.charge;
          currentword.address = pc.pad;
          currentword.dilogic = dilogic ;
          currentword.row = row;
          mPayload.push_back(currentword.mDataWord);
          nwInSegment++;
          nPad++ ;
          nwRow++;
        }
printf("Filling EoE: (row=%d, dil=%d), nwEoE=%d, payload=%d\n",row,dilogic,nPad,mPayload.size()) ;        
        EoEWord we = {0};
        we.nword= nPad;  
        we.dilogic = dilogic ;
        we.row = row ;
        we.checkbit = 1;
        mPayload.push_back(we.mDataWord) ;
        nwInSegment++;        
        nwRow++;
      }
      if(row%8==7){  
        SegMarkerWord w={0};
        w.row=row;
        w.nwords=nwInSegment;
        w.marker=2736;
        mPayload.push_back(w.mDataWord);
printf(" Serment word=%d, nw=%d, payload=%d \n",w,nwInSegment,mPayload.size()) ;        
        nwInSegment=0;
        nwRow++;
      }
      //Now we know number of words, update rawMarker
      RowMarkerWord wr={0};
      wr.marker = 13992 ;
      wr.nwords = nwRow-1;
printf(" Row word=%d \n",wr) ;        
      mPayload[posRowMarker]=wr.mDataWord;
    }
 

    //rewrite header with size set to or trailer will be added by framework?
//    header.fSize=sizeof(header)+cntL*sizeof(w32); ddlL->Seekp(0); ddlL->WriteBuffer((char*)&header,sizeof(header)); delete ddlL;
  
    // register output data
    LOG(DEBUG1) << "Adding payload with size " << mPayload.size() << " (" << mPayload.size() / 4 << " ALTRO words)";

printf("payload size=%d \n",mPayload.size()) ;
for(int iii=0; iii<mPayload.size(); iii++){printf(", %d",mPayload[iii]) ; }  


    short crorc=0, link=ddl;
    mRawWriter->addData(ddl, crorc, link, 0, trg.getBCData(), gsl::span<char>(reinterpret_cast<char*>(mPayload.data()),mPayload.size()*sizeof(uint32_t)));
  }
  return true;
}

// int RawWriter::carryOverMethod(const header::RDHAny* rdh, const gsl::span<char> data,
//                                const char* ptr, int maxSize, int splitID,
//                                std::vector<char>& trailer, std::vector<char>& header) const
// {
//   int offs = ptr - &data[0]; // offset wrt the head of the payload
//   // make sure ptr and end of the suggested block are within the payload
//   assert(offs >= 0 && size_t(offs + maxSize) <= data.size());

//   // Read trailer template from the end of payload
//   // gsl::span<const uint32_t> payloadwords(reinterpret_cast<const uint32_t*>(data.data()), data.size() / sizeof(uint32_t));
//   auto rcutrailer = RCUTrailer::constructFromPayload(data);

//   int sizeNoTrailer = maxSize - rcutrailer.getTrailerSize() * sizeof(uint32_t);
//   // calculate payload size for RCU trailer:
//   // assume actualsize is in byte
//   // Payload size is defined as the number of 32-bit payload words
//   // -> actualSize to be converted to size of 32 bit words
//   auto payloadsize = sizeNoTrailer / sizeof(uint32_t);
//   rcutrailer.setPayloadSize(payloadsize);
//   auto trailerwords = rcutrailer.encode();
//   trailer.resize(trailerwords.size() * sizeof(uint32_t));
//   memcpy(trailer.data(), trailerwords.data(), trailer.size());
//   // Size to return differs between intermediate pages and last page
//   // - intermediate page: Size of the trailer needs to be removed as the trailer gets appended
//   // - last page: Size of the trailer needs to be included as the trailer gets replaced
//   int bytesLeft = data.size() - (ptr - &data[0]);
//   bool lastPage = bytesLeft <= maxSize;
//   int actualSize = maxSize;
//   if (!lastPage) {
//     actualSize = sizeNoTrailer;
//   }
//   return actualSize;
// }
