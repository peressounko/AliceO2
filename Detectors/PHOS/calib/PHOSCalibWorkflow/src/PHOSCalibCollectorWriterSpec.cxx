// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#ifndef O2_CALIBRATION_TOFCALIB_COLLECTOR_WRITER_H
#define O2_CALIBRATION_TOFCALIB_COLLECTOR_WRITER_H

/// @file   TOFCalibCollectorWriterSpec.h
/// @brief  Device to write to tree the information for TOF time slewing calibration.

#include "TOFCalibration/TOFCalibCollector.h"
#include "DataFormatsTOF/CalibInfoTOFshort.h"
#include <TTree.h>
#include <gsl/span>
#include <ctime>

using namespace o2::framework;

//________________________________________________________________
void PHOSCalibCollectorWriter::createAndOpenFile(){

   time_t now = time(0);
   tm *ltm = localtime(&now);

  TString filename = TString::Format("collPHOS_%d%d%d%d%d.root", 1 + ltm->tm_min,1 + ltm->tm_hour,ltm->tm_mday,1 + ltm->tm_mon, 1970 + ltm->tm_year);
  LOG(DEBUG) << "opening file " << filename.Data();
  mfileOutCells.reset(TFile::Open(TString::Format("%s", filename.Data()), "RECREATE"));

  if(!mFileTimeHisto ){
    TString filenameT = TString::Format("collPHOSTime_%d%d%d%d%d.root", 1 + ltm->tm_min,1 + ltm->tm_hour,ltm->tm_mday,1 + ltm->tm_mon, 1970 + ltm->tm_year);
    LOG(DEBUG) << "opening file " << filenameT.Data();
    mFileTimeHisto = std::make_unique(TFile::Open(TString::Format("%s", filenameT.Data()), "RECREATE"));
  }
  else{
    if(!mFileTimeHisto->IsOpen()){
      TString filenameT = TString::Format("collPHOSTime_%d%d%d%d%d.root", 1 + ltm->tm_min,1 + ltm->tm_hour,ltm->tm_mday,1 + ltm->tm_mon, 1970 + ltm->tm_year);
      LOG(DEBUG) << "opening file " << filenameT.Data();
      mFileTimeHisto.reset(TFile::Open(TString::Format("%s", filenameT.Data()), "RECREATE"));
    }
  }

  if(!mFileEnergyHisto ){
    TString filenameE = TString::Format("collPHOSEnergy_%d%d%d%d%d.root", 1 + ltm->tm_min,1 + ltm->tm_hour,ltm->tm_mday,1 + ltm->tm_mon, 1970 + ltm->tm_year);
    LOG(DEBUG) << "opening file " << filenameE.Data();
    mFileEnergyHisto = std::make_unique(TFile::Open(TString::Format("%s", filenameE.Data()), "RECREATE"));
  }
  else{
    if(!mFileEnergyHisto->IsOpen()){
      TString filenameT = TString::Format("collPHOSEnergy_%d%d%d%d%d.root", 1 + ltm->tm_min,1 + ltm->tm_hour,ltm->tm_mday,1 + ltm->tm_mon, 1970 + ltm->tm_year);
      LOG(DEBUG) << "opening file " << filenameE.Data();
      mFileEnergyHisto.reset(TFile::Open(TString::Format("%s", filenameE.Data()), "RECREATE"));
    }
  }
}

//________________________________________________________________
void PHOSCalibCollectorWriter::init(o2::framework::InitContext& ic){
  mChank = 0 ;
  createAndOpenFile();
}

//________________________________________________________________
void PHOSCalibCollectorWriter::run(o2::framework::ProcessingContext& pc){
  
   sendOutput(pc.outputs());
}

//________________________________________________________________
void PHOSCalibCollectorWriter::endOfStream(o2::framework::EndOfStreamContext& ec){
  mIsEndOfStream = true;
  sendOutput(ec.outputs());
}

//________________________________________________________________
void PHOSCalibCollectorWriter::sendOutput(DataAllocator& output){
  
  // This is to fill the output files
  // If this is last call, 
  // fill files with Time histograms and interation 0 energy calibration histograms
  if(mIsEndOfStream){ //only final histograms are wrote
    auto histoTlist = pc.inputs().get<gsl::span<o2::dataformats::CalibInfoTOFshort>>("histoTlist");
    mFileTimeHisto->cd() ;
    for(auto &h : histoTlist) h->Write(0,TObject::kOverwrite) ;

    auto histoElist = pc.inputs().get<gsl::span<o2::dataformats::CalibInfoTOFshort>>("histoElist");
    mFileEnergyHisto->cd() ;
    for(auto &h : histoElist) h->Write(0,TObject::kOverwrite) ;
  }

  // For each call write next chank of digits for later re-calibrations
  //Calibration digits  
  auto digits = pc.inputs().get<gsl::span<uint32_t>>("calibrationDigits");
  mfileOutCells->cd();
  mfileOutCells->WriteObjectAny(&digits,"std::vector<uint32_t>",Form("CalibrationDigits%d",mChank++)) ;

  //Do not create too large output digit files
  if(mIsEndOfStream || mfileOutCells->GetBytesWritten()>kMaxFileSize){
    mfileOutCells->Close() ;
    if (!mIsEndOfStream) {
     mChank=0; 
     createAndOpenFile();
    }
  }
}

o2::framework::DataProcessorSpec getPHOSCalibCollectorWriterSpec()
{
  std::vector<InputSpec> inputs;
  inputs.emplace_back("histoTlist", o2::header::gDataOriginPHS, "TIMEHISTOS");
  inputs.emplace_back("histoElist", o2::header::gDataOriginPHS, "ENERGYHISTOS");
  inputs.emplace_back("calibrationDigits", o2::header::gDataOriginPHS, "CALIBCELLS");

  std::vector<OutputSpec> outputs; // empty

  return DataProcessorSpec{
    "calib-phoscalib-collector-writer",
    inputs,
    outputs,
    AlgorithmSpec{adaptFromTask<o2::calibration::PHOSCalibCollectorWriter>()},
    Options{}};
}

