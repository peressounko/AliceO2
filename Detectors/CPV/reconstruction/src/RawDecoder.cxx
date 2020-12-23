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
printf("payload size=%d \n",payloadwords.size()) ;
for(int iii=0; iii<payloadwords.size(); iii++){printf(", %d",payloadwords[iii]) ; }  printf("\n") ;
  if(payloadwords.size()==0){
    mErrors.emplace_back(ddl,0,0,0,kNO_PAYLOAD) ;//add error
    LOG(ERROR)<<"Empty payload for DDL="<<ddl ;
    return kNO_PAYLOAD; 
  }

  // if(readRCUTrailer()!=kOK){
  //   LOG(ERROR) << "can not read RCU trailer for DDL " << ddl ;
  //   return kRCU_TRAILER_ERROR;
  // }

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
printf("reading channels\n") ;  
  mChannelsInitialized = false;
  auto& header = mRawReader.getRawHeader();
  short ddl = o2::raw::RDHUtils::getFEEID(header); //Current fee/ddl

  auto &payloadwords = mRawReader.getPayload();  
  //start reading from the end
  auto currentWord=payloadwords.rbegin() ;
  while(currentWord!=payloadwords.rend() ){
    SegMarkerWord sw={*currentWord++}; //first get value, then increment
printf("... Read Serments, sw=%d, nWords=%d \n",sw.mDataWord,sw.nwords) ;    
    if(sw.marker!=2736){ //error
printf(".... ===>incorrect Segment: %d \n",sw.marker) ;        
      mErrors.emplace_back(ddl,17,2,0,kSEGMENT_HEADER_ERROR) ; //add error for non-existing row
      //try adding this as padWord
      addDigit(sw.mDataWord,ddl) ;
      continue;
    }
    short nSegWords=sw.nwords;
    short currentRow=sw.row;
    short nEoE=0;
    while(nSegWords>0 && currentWord!=payloadwords.rend() ){
      EoEWord ew ={*currentWord++} ;
      nSegWords--;
      if(ew.checkbit != 1){ //error
printf(".........===>error EoE \n") ;        
        mErrors.emplace_back(ddl,currentRow,11,0,kEOE_HEADER_ERROR) ; //add error
        //try adding this as padWord
        addDigit(ew.mDataWord,ddl) ;
        continue;
      }
      nEoE++;
      short nEoEwords=ew.nword ;
printf("..........EoE words=%d,SegW=%d\n",nEoEwords,nSegWords) ;        
      short currentDilogic = ew.dilogic;
      if(ew.row!=currentRow){
printf("..........===>Row in EoE=%d != expected row %d\n",ew.row,currentRow) ;
        mErrors.emplace_back(ddl,currentRow,currentDilogic,0,kEOE_HEADER_ERROR) ;//add error
        //try adding this as padWord
        addDigit(ew.mDataWord,ddl) ;
        continue;
      }
      if(currentDilogic<0 || currentDilogic>10){
printf("..........===>Dilogic in EoE=%d \n",currentDilogic) ;
        mErrors.emplace_back(ddl,currentRow,currentDilogic,0,kEOE_HEADER_ERROR) ; //add error
        //try adding this as padWord
        addDigit(ew.mDataWord,ddl) ;
        continue;
      }
      while(nEoEwords>0 && currentWord!=payloadwords.rend()){       
        PadWord pad = {*currentWord++} ;
        nEoEwords--;
        nSegWords--;
        if(pad.zero!=0){
printf("            bad pad \n") ;            
          mErrors.emplace_back(ddl,currentRow,currentDilogic,49,kPADERROR); //add error and skip word
          continue ;
        }
        //check paw/pad indexes
        if(pad.row!=currentRow || pad.dilogic!=currentDilogic){
printf("==>RawPad  %d!=%d, dilogicPad=%d != currentDilogic=%d \n",pad.row,currentRow,pad.dilogic,currentDilogic) ;      
          mErrors.emplace_back(ddl,short(pad.row),short(pad.dilogic),short(pad.address),kPadAddress) ; //add error and skip word
          //do not skip, try adding using info from pad
        }
        addDigit(pad.mDataWord,ddl) ;
      } //pads in EoE 
printf("nEoE=%d \n",nEoE) ;      
      if(nEoE%10==0){ // kNDilogic = 10;   ///< Number of dilogic per row 
        if(currentWord!=payloadwords.rend()){ //Read row HEader
          RowMarkerWord rw={*currentWord++} ;
          nSegWords--;
          currentRow--;
printf("Read Row header: rw=%d, mark=%d =? 13992\n",rw.mDataWord,rw.marker) ;        
          if(rw.marker!=13992){
printf("   ===>Error in row=%d marker: %d \n",rw.mDataWord, rw.marker) ;            
           mErrors.emplace_back(ddl,currentRow,11,0,kPadAddress) ; //add error and skip word
           //try adding digit assuming this is pad word
           addDigit(rw.mDataWord,ddl) ;
          }
        }
      }
    } // in Segment
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

void RawDecoder::addDigit(uint32_t w, short ddl){
  
  PadWord pad = {w} ;
  if(pad.zero!=0){
    return ;
  }
  short rowPad = pad.row; 
  short dilogicPad=pad.dilogic;
  short hw=pad.address;
  unsigned short absId; 
  o2::cpv::Geometry::hwaddressToAbsId(ddl, rowPad, dilogicPad, hw, absId) ;
if(absId>30720){
printf("ADDRESS: ddl=%d, row=%d, dil=%d, hw=%d, absId=%d \n",ddl, rowPad, dilogicPad, hw, absId) ; 
}

  AddressCharge ac={0} ;
  ac.Address = absId;
  ac.Charge = pad.charge ;
  mDigits.push_back(ac.mDataWord) ;
 
}