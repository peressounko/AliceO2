// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

/// \class Pedestals
/// \brief CCDB container for the full set of PHOS calibration coefficients
/// \author Dmitri Peresunko, RRC Kurchatov institute
/// \since Aug. 1, 2019
///
///

#ifndef PHOS_PEDESTALS_H
#define PHOS_PEDESTALS_H

#include <array>
#include "TObject.h"

class TH1;

namespace o2
{

namespace phos
{

class Pedestals
{
 public:
  /// \brief Constructor
  Pedestals() = default;

  /// \brief Constructor for tests
  Pedestals(int test);

  /// \brief Destructor
  ~Pedestals() = default;

  /// \brief Get pedestal
  /// \param cellID Absolute ID of cell
  /// \return pedestal for the cell
  short getPedestal(short cellID) const { return short(mPedestals.at(cellID)); }

  /// \brief Set pedestal
  /// \param cellID Absolute ID of cell
  /// \param c is the pedestal (expected to be in range <254)
  void setPedestal(short cellID, short c) { mPedestals[cellID] = char(c); }


  /// \brief Set pedestals from 1D histogram with cell absId in x axis
  /// \param 1D(NCHANNELS) histogram with calibration coefficients
  /// \return Is successful
  bool setPedestals(TH1* h);

 private:
  static constexpr short NCHANNELS = 14337;  ///< Number of channels starting from 1
  std::array<char, NCHANNELS> mPedestals;    ///< Container for pedestals

  ClassDefNV(Pedestals, 1);
};

} // namespace phos

} // namespace o2
#endif
