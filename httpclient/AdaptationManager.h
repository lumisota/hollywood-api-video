/*
 * AdaptationManager.h
 *****************************************************************************
 * Copyright (C) 2014 Warsaw University of Technology
 *
 * Created on: March 15, 2015
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
 
#ifndef AdaptationManager_H
#define AdaptationManager_H

#include <string>
#include <boost/random.hpp>
#include <vector>
//---------------------------------------------------------------------------//
// AdaptationManager
//---------------------------------------------------------------------------//
class AdaptationManager{
  public:
    AdaptationManager (std::vector<uint64_t> reprezentations, uint64_t segmentDuration, uint64_t maxBuffLength);
    virtual void     addRtt(uint64_t rtt)=0;                       // [us]
    virtual void     addData(uint64_t segmentSize, uint64_t sdt,   // [B], [us],
                       uint64_t bufferOccupanecy)=0;               // [us]
    virtual int      adaptation()=0;                              
    virtual uint64_t getBuffLen();                                 // [us]
    virtual uint64_t getRepresentation();
    virtual int      getRepresentationIdx();
    virtual ~AdaptationManager();

  protected:	
    std::vector<uint64_t> reps;
    uint64_t segmentLen;                // [us]
    uint64_t maxBuffLength;             // [us]
    int  currentRepIdx; 
};

#endif
