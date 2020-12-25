// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#include "CPVCalibWorkflow/CPVPedestalCalibDevice.h"
#include "CPVCalibWorkflow/CPVHGLGRatioCalibDevice.h"
#include "Framework/DataProcessorSpec.h"

using namespace o2::framework;



// // we need to add workflow options before including Framework/runDataProcessing
void customize(std::vector<o2::framework::ConfigParamSpec>& workflowOptions)
{
  // option allowing to set parameters
  workflowOptions.push_back(ConfigParamSpec{"use-ccdb", o2::framework::VariantType::Bool, false, {"enable access to ccdb cpv calibration objects"}});
  workflowOptions.push_back(ConfigParamSpec{"pedestals", o2::framework::VariantType::Bool, true, {"do pedestal calculation"}});
  workflowOptions.push_back(ConfigParamSpec{"gains", o2::framework::VariantType::Bool, false, {"do gain calculation"}});
}

// ------------------------------------------------------------------
// we need to add workflow options before including Framework/runDataProcessing
#include "Framework/runDataProcessing.h"

WorkflowSpec defineDataProcessing(ConfigContext const& configcontext)
{
  WorkflowSpec specs;
  auto useCCDB     = configcontext.options().get<bool>("use-ccdb");
  auto doPedestals = configcontext.options().get<bool>("pedestals");
  auto doGain = configcontext.options().get<bool>("gain");
  if (doPedestals && doGain) {
    LOG(FATAL) << "Can not run pedestal and gain calibration simulteneously";
  }

  LOG(INFO) << "CPV Calibration workflow: options";
  LOG(INFO) << "useCCDB = " << useCCDB;
  if (doPedestals) {
    LOG(INFO) << "pedestals ";
    specs.emplace_back(o2::cpv::getPedestalCalibSpec(useCCDB));
  }
  else{
    if (doHgLgRatio) {
      LOG(INFO) << "gain ";
      // specs.emplace_back(o2::cpv::getGainCalibSpec(useCCDB));
    }
  }
  return specs;
}
