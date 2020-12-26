// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include <memory>
#include <string>
#include <vector>
#include "Framework/Logger.h"

#include <boost/program_options.hpp>

#include <TFile.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <TSystem.h>

#include "CommonUtils/ConfigurableParam.h"
#include "CommonUtils/StringUtils.h"
#include "DataFormatsPHOS/Digit.h"
#include "DataFormatsPHOS/TriggerRecord.h"
#include "PHOSBase/Geometry.h"
#include "PHOSSimulation/RawWriter.h"

namespace bpo = boost::program_options;

int main(int argc, const char** argv)
{
  bpo::variables_map vm;
  bpo::options_description opt_general("Usage:\n  " + std::string(argv[0]) +
                                       " <cmds/options>\n"
                                       "  Tool will encode phos raw data from input file\n"
                                       "Commands / Options");
  bpo::options_description opt_hidden("");
  bpo::options_description opt_all;
  bpo::positional_options_description opt_pos;

  try {
    auto add_option = opt_general.add_options();
    add_option("help,h", "Print this help message");
    add_option("verbose,v", bpo::value<uint32_t>()->default_value(0), "Select verbosity level [0 = no output]");
    add_option("input-dir,i", bpo::value<std::string>()->default_value("PHSCalib/"), "Specifies dir with calib digit input file(s).");
    add_option("input-file-pattern,p", bpo::value<std::string>()->default_value("collPHOS_*.root"), "Specifies pattern for input file(s).");
    add_option("calib-file,c", bpo::value<std::string>()->default_value("PHOSCalibE.root"), "Specifies file with energy calib.");
    add_option("output-dir,o", bpo::value<std::string>()->default_value("./"), "output directory for raw data");
    add_option("debug,d", bpo::value<uint32_t>()->default_value(0), "Select debug output level [0 = no debug output]");
    add_option("configKeyValues", bpo::value<std::string>()->default_value(""), "comma-separated configKeyValues");

    opt_all.add(opt_general).add(opt_hidden);
    bpo::store(bpo::command_line_parser(argc, argv).options(opt_all).positional(opt_pos).run(), vm);

    if (vm.count("help") || argc == 1) {
      std::cout << opt_general << std::endl;
      exit(0);
    }

  } catch (bpo::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl
              << std::endl;
    std::cerr << opt_general << std::endl;
    exit(1);
  } catch (std::exception& e) {
    std::cerr << e.what() << ", application will now exit" << std::endl;
    exit(2);
  }

  o2::conf::ConfigurableParam::updateFromString(vm["configKeyValues"].as<std::string>());

  auto calibfilename = vm["input-file"].as<std::string>(),
       inputdir = vm["input-dir"].as<std::string>(),
       inputpattern = vm["input-file-pattern"].as<std::string>(),
       outputdir = vm["output-dir"].as<std::string>();

  // if needed, create output directory
  if (gSystem->AccessPathName(outputdir.c_str())) {
    if (gSystem->mkdir(outputdir.c_str(), kTRUE)) {
      LOG(FATAL) << "could not create output directory " << outputdir;
    } else {
      LOG(INFO) << "created output directory " << outputdir;
    }
  }

  //Get new calibration and bad map
  ...

  //Check input dir
  if (!gSystem->AccessPathName(inputdir.c_str())) {
    LOG(FATAL) << "can not find input directory " << inputdir;
  }
  //Prepare list of files
  std::vector<std::string> infiles ;
  list_files(inputdir, inputpattern, infiles) ;

  auto ifile = infiles.begin() ;
  while(ifile!=infiles.end()){
    std::unique_ptr<TFile> digitfile(TFile::Open(*ifile, "READ"));
    TList * keylist =  digitfile->GetListOfKeys() ;
    TIter next(digitfile->GetListOfKeys());
    while ((TKey *key=(TKey*)next())) {
      std::vector<uint32_t> * digits = (std::vector<uint32_t>)digitfile->Get(key) ;
      processDigits(digits) ;
      //should we delete digits now???
      //Or root will do it for us?
   }

   //Write output histograms


  }

  auto treereader = std::make_unique<TTreeReader>(static_cast<TTree*>(digitfile->Get("o2sim")));
  TTreeReaderValue<std::vector<o2::phos::Digit>> digitbranch(*treereader, "PHOSDigit");
  TTreeReaderValue<std::vector<o2::phos::TriggerRecord>> triggerbranch(*treereader, "PHOSDigitTrigRecords");

  o2::phos::RawWriter::FileFor_t granularity = o2::phos::RawWriter::FileFor_t::kFullDet;
  if (filefor == "all") {
    granularity = o2::phos::RawWriter::FileFor_t::kFullDet;
  } else if (filefor == "link") {
    granularity = o2::phos::RawWriter::FileFor_t::kLink;
  }

  o2::phos::RawWriter rawwriter;
  rawwriter.setOutputLocation(outputdir.data());
  rawwriter.setFileFor(granularity);
  rawwriter.init();

  // Loop over all entries in the tree, where each tree entry corresponds to a time frame
  for (auto en : *treereader) {
    rawwriter.digitsToRaw(*digitbranch, *triggerbranch);
  }
  rawwriter.getWriter().writeConfFile("PHS", "RAWDATA", o2::utils::concat_string(outputdir, "/PHSraw.cfg"));
}
void list_files(const char *dirname="./", const char *pattern="collPHOS_*.root", std::vector<std::string> &vnames) { 
  TSystemDirectory dir(dirname, dirname); 
  TList *files = dir.GetListOfFiles(); 
  if (files){ 
    TSystemFile *file; 
    TString fname; 
    TIter next(files); 
    while ((file=(TSystemFile*)next())) { 
      fname = file->GetName(); 
      if (!file->IsDirectory() && fname.Index(pattern) > -1) { 
        vnames.emplace_back(fname.Data()) ;
      } 
    } 
  } 
}

void  processDigits(std::vector<uint32_t> *digits){
  //List of digits is event header followed by digits

  auto it= digits->begin() ;
  int nclu=0; 
  std::vector<o2::phos::FullCluster> vclusters;
  while(it!=digits->end()){
    EventHeader eh={*it} ;
    if(eh.mMarker!=16383){
      LOG(ERROR) << "Key corrupted, ckipping" ;
      return ;
    }

    //now read digits
    it++;
    bool sameEvent=true;
    short currentClu=-1; 
    vclusters.clear() ;
    nEventStart=mBuffer.size();
    while(it!=digits->end() && sameEvent){
      CalibDigit d={*it} ;
      if(d.mAddress==16383){ //header from next event
        sameEvent=false;
        continue;
      }
      it++;
      short absId= d.mAddress;
      float e=d.mAdcAmp*mCalibParam.getGain(absId) ;
      if(d.mHgLg){
        e*=mCalibParam.getHGLGRatio(absId) ;
      }

      if(d.mCluster!=currentClu){ //start new cluster
        vclusters.emplace_back(absId,e,0,-1,1.) ;
        clu=vclusters.back() ;
      }
      else{
        clu.addDigit(absId,e,0,-1,1.) ;
      } //next event

      //analyze collected clusters
      //calculate parameters and add
      //TLorentsVectors to curcle buffer
      for(auto &clu :vclusters.size()){
        clu->evalAll() ;
        //calculate 4-miomentum

        //Real and Mixed inv mass distributions
        TLorentzVector v(px,py,pz,e) ;
        for(short ip=mBuffer.size(); --ip; ){
          const TLorentzVector &vp = mBuffer.getEntry(ip) ;
          TLorentzVector sum=v+vp;
          if(ip>nEventStart){ //same (real) event
            if(isGood){
              mHistos[kReInvMassNonlin]->Fill(e,sum.M()) ;
            }
            if(sum.Pt()>kPtMin){
              mHistos[kReInvMassPerCell]->Fill(absId,sum.M()) ;
            }
          }
          else{ //Mixed
            if(isGood){
              mHistos[kMiInvMassNonlin]->Fill(e,sum.M()) ;
            }
            if(sum.Pt()>kPtMin){
              mHistos[kMiInvMassPerCell]->Fill(absId,sum.M()) ;
            }
          }
        }

        //Add to list ot partners only if cluster is good
        if(isGood){
          nEventStart+= mEventBuffer.emplace_back(v) ; //do not change nEvent Start if buffer still increases
          nEventStart--;                               //or decrease if buffer already full
        }
      }
    }
  }
}
