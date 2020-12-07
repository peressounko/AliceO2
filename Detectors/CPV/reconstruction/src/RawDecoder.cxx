// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
#include <FairLogger.h>
#include "CPVReconstruction/RawReaderMemory.h"
#include "CPVReconstruction/RawDecoder.h"
#include "DataFormatsCPV/RawFormats.h"
#include "InfoLogger/InfoLogger.hxx"
#include "DetectorsRaw/RDHUtils.h"
#include "CPVBase/Geometry.h"

using namespace o2::cpv;

RawDecoder::RawDecoder(RawReaderMemory& reader) : mRawReader(reader),
                                                      mRCUTrailer(),
                                                      mChannelsInitialized(false)
{
}

RawErrorType_t RawDecoder::decode()
{

  auto& header = mRawReader.getRawHeader();
  short ddl = o2::raw::RDHUtils::getFEEID(header);
  mDigits.clear() ;

  auto payloadwords = mRawReader.getPayload();
  if(payloadwords.size()==0){
    mErrors.emplace_back(ddl,0,0,0,kNO_PAYLOAD) ;//add error
    LOG(ERROR)<<"Empty payload for DDL="<<ddl ;
    return kNO_PAYLOAD; 
  }

  if(readRCUTrailer()!=kOK){
    LOG(ERROR) << "can not read RCU trailer for DDL " << ddl ;
    return kRCU_TRAILER_ERROR;
  }

  return readChannels();
}

RawErrorType_t RawDecoder::readRCUTrailer()
{
  gsl::span<const char> payload(reinterpret_cast<const char*>(mRawReader.getPayload().data()), mRawReader.getPayload().size()*sizeof(uint32_t));
  mRCUTrailer.constructFromRawPayload(payload);
  return kOK ;
}

RawErrorType_t RawDecoder::readChannels()
{
  mChannelsInitialized = false;
  auto& header = mRawReader.getRawHeader();
  short ddl = o2::raw::RDHUtils::getFEEID(header); //Current fee/ddl

  auto &payloadwords = mRawReader.getPayload();  
  auto currentword = payloadwords.begin();

  while(currentword!=payloadwords.end()){
    SegMarkerWord sw={*currentword} ;
    if(sw.marker!=2736){ //error
      mErrors.emplace_back(ddl,17,2,0,kSEGMENT_HEADER_ERROR) ; //add error for non-existing row
      currentword++ ; //Try to read words till next Segment or end of payload.
      continue;
    }
    short nSegWords=sw.nwords;
    short currentRow=sw.row;
    currentword++ ;
    nSegWords--;
    while(currentword!=payloadwords.end() && nSegWords>0){ //read rows
      RowMarkerWord rw = {*currentword} ;
      if(rw.marker!= 13992){ //error
        mErrors.emplace_back(ddl,currentRow,11,0,kROW_HEADER_ERROR) ; //add error for non-existing dilogic
        currentword++ ; //Try to read words till next Segment or end of payload.
        nSegWords--;
        continue ;
      }
      currentword++ ;
      nSegWords--;
      short nRowWords = rw.nwords;
      while(currentword!=payloadwords.end() && nSegWords>0 && nRowWords>0){
        EoEWord ew ={ *currentword} ;
        if(ew.checkbit != 1){ //error
          mErrors.emplace_back(ddl,currentRow,11,0,kEOE_HEADER_ERROR) ; //add error
          //go to next EoE
          currentword++ ; //Try to read words till next EoE or end of payload.
          nSegWords--;
          nRowWords--;
          continue;
        }
        currentword++ ;
        nSegWords--;
        nRowWords--;
        short currentDilogic = ew.dilogic;
        if(ew.row!=currentRow){
          mErrors.emplace_back(ddl,currentRow,currentDilogic,0,kEOE_HEADER_ERROR) ;//add error
          //move to next EoE
          currentword++ ; //Try to read words till next EoE or end of payload.
          nSegWords--;
          nRowWords--;
          continue;
        }
        if(currentDilogic<0 || currentDilogic>10){
          mErrors.emplace_back(ddl,currentRow,currentDilogic,0,kEOE_HEADER_ERROR) ; //add error
          currentword++ ; //Try to read words till next EoE or end of payload.
          nSegWords--;
          nRowWords--;
          continue;
        }
        for(short i=ew.nword; i>0 && currentword!=payloadwords.end() ; i--){
          currentword++ ;
          nSegWords--;

          PadWord pad;
          pad.mDataWord = *currentword ;
          if(pad.zero!=0){
            mErrors.emplace_back(ddl,currentRow,currentDilogic,49,kPADERROR); //add error and skip word
            continue ;
          }
          //check paw/pad indexes
          short rowPad = pad.row; 
          short dilogicPad=pad.dilogic;
          short hw=pad.address;
          if(rowPad!=currentRow || dilogicPad!=currentDilogic){
            mErrors.emplace_back(ddl,rowPad,dilogicPad,hw,kPadAddress) ; //add error and skip word
            continue;
          }
          short absId; 
          o2::cpv::Geometry::hwaddressToAbsId(ddl, rowPad, dilogicPad, hw, absId) ;
          AddressCharge ac={0} ;
          ac.Address = absId;
          ac.Charge = pad.charge ;
          mDigits.push_back(ac.mDataWord) ;
        }  
      }
    }
  }

  mChannelsInitialized = true;
}

const RCUTrailer& RawDecoder::getRCUTrailer() const
{
  if (!mRCUTrailer.isInitialized()){
    LOG(ERROR) << "RCU trailer not initialized" ;
  }
  return mRCUTrailer;
}

const std::vector<uint32_t>& RawDecoder::getDigits() const
{
  if (!mChannelsInitialized)
    LOG(ERROR) << "Channels not initialized" ;
  return mDigits;
}
