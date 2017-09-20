/*
 * AdaptationManagerABMAplus.h
 *****************************************************************************
 * Copyright (C) 2015 Warsaw University of Technology
 *
 * Created on: May 15, 2015
 * Authors: Piotr Wisniewski     <pwisniewsk@tele.pw.edu.pl>
 *          Andrzej Beben        <abeben@tele.pw.edu.pl>
 *          Jordi Mongay Batalla <jordim@tele.pw.edu.pl>
 *          Piotr Krawiec        <pkrawiec@tele.pw.edu.pl>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
 
#ifndef AdaptationManagerABMAplus_H
#define AdaptationManagerABMAplus_H

#include "AdaptationManager.h"
#include <vector>
#include <fstream>
#include <iostream>
#include <string>
#include <deque>


/*
  a) addData() method intended for transferring measurement data to the module, 
  b) adaptation() method, which recalculates an adaptation formula, and 
  c) getRepresentation() method(), which returns current media representation determined by adaptation engine.
  d) getBuffLen() method to return value of appropriate buffer capacity calculated by ABMA
 */
namespace dash{
  namespace http{
    namespace abma_plus_constants{
      const uint64_t DEFAULT_SEGMENT_DURATION = 1000000;                // [us]
      const uint64_t DEFAULT_MAX_BUFF_LENGTH  = 
                                         32 * DEFAULT_SEGMENT_DURATION; // [us]   
      const uint64_t DEFAULT_RESERVOIR_LENGTH = 
                                         16 * DEFAULT_SEGMENT_DURATION; // [us]   
      const uint16_t DEFAULT_MIN_ENTRIES      = 10; /*For keeping timing information*/
      const uint16_t DEFAULT_MAX_ENTRIES      = 50;
      const float    DEFAULT_BETA             = 0.1;   
      const bool DEBUG_ADAPTATION             = false;
    }
//---------------------------------------------------------------------------//
// AdaptationManagerABMAplus
//---------------------------------------------------------------------------//
    class AdaptationManagerABMAplus: public AdaptationManager {
     public:
      void     addRtt(uint64_t rtt);                       // [us]
      void     addData(uint64_t segmentSize, uint64_t sdt, // [B], [us],
                       uint64_t bufferOccupanecy);         // [us]
      int      adaptation();    // returns 0 if buffer length calculated
      uint64_t getBuffLen();                               // [us]
      AdaptationManagerABMAplus (std::vector<uint64_t> representations, 
                uint64_t segmentDuration = 
                         abma_plus_constants::DEFAULT_SEGMENT_DURATION, // [us]
                uint64_t maxBuffLength   = 
                         abma_plus_constants::DEFAULT_MAX_BUFF_LENGTH, // [us]
                uint64_t reservoirLen    = 
                         abma_plus_constants::DEFAULT_RESERVOIR_LENGTH, //[us]
                uint16_t minEntries = abma_plus_constants::DEFAULT_MIN_ENTRIES, 
                uint16_t maxEntries = abma_plus_constants::DEFAULT_MAX_ENTRIES,
                float    beta       = abma_plus_constants::DEFAULT_BETA

      );
      ~AdaptationManagerABMAplus();
     protected:	
       const uint64_t reservoirLen;
       const uint16_t minEntries; 
       const uint16_t maxEntries; 
       const float    beta;
       std::deque<float> sample;
       int bMap[101][101];
       uint64_t bufferOccupancy;
       int b;               
     }; 	

  } // namespace http
} // namespace dash   
#endif
