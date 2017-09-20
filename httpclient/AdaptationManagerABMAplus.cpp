/*
 * AdaptationManagerABMA.cpp
 *****************************************************************************
 * Copyright (C) 2015 Warsaw University of Technology
 *
 * Created on: May 15, 2014
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

#include "AdaptationManagerABMAplus.h"

using namespace dash::http::abma_plus_constants;
using namespace dash::http;
using namespace std;


//---------------------------------------------------------------------------//
// AdaptationManagerABMAplus
//---------------------------------------------------------------------------//								  

AdaptationManagerABMAplus::AdaptationManagerABMAplus(
    vector<uint64_t> representations,  uint64_t segmentDuration,
    uint64_t maxBuffLength, uint64_t reservoirLen, uint16_t minEntries, 
    uint16_t maxEntries, float beta):
      AdaptationManager(representations, segmentDuration, maxBuffLength),
      reservoirLen(reservoirLen),
      minEntries(minEntries),
      maxEntries(maxEntries),
      beta(beta)
{  
  // open rate-map file 
  ifstream mapFile;
  mapFile.open("buffer_map.txt", fstream::in);

  currentRepIdx = 0;

  if (!mapFile.is_open())
  {
      cerr << "Failed to open file: " << "buffer_map.txt" << endl;
  }
  string stringLine;
  int ovStart, cvStart, ovStep, cvStep, ovEnd, cvEnd;
  ovStart = cvStart = ovStep = cvStep = 1;
  ovEnd = cvEnd = 100;

  int ov, cv, b;
  for (ov=ovStart; ov<=ovEnd; ov+=ovStep){
    getline(mapFile, stringLine);
    stringstream streamLine(stringLine);
    for (cv=cvStart; cv<=cvEnd; cv+=cvStep){
      streamLine >> b;
      bMap[ov][cv]=b;   
    }
  } 
    
  if (mapFile.is_open()) 
    mapFile.close();

  cout << "Used Adaptation Manager: ABMAplus" << endl;
  
}

AdaptationManagerABMAplus::~AdaptationManagerABMAplus(){}


void AdaptationManagerABMAplus::addData(uint64_t segmentSize, uint64_t sdt, 
                                                      uint64_t buffOccupancy)
{
  this->bufferOccupancy = buffOccupancy;
  if (sample.size() == maxEntries)
    sample.pop_front();
  this->sample.push_back(float(sdt));

}

void AdaptationManagerABMAplus::addRtt(uint64_t rtt){}


int AdaptationManagerABMAplus::adaptation(){
  int minSampleSize = minEntries;
  if (sample.size()<minSampleSize)
    return 1;

  int maxBuffSize = floor(maxBuffLength / segmentLen);
  int reservoirSize = floor(reservoirLen / segmentLen);

  float sum = accumulate(sample.begin(), sample.end(), 0.0);
  float mean = sum / sample.size();
    
  vector<float> diff(sample.size());
  transform(sample.begin(), sample.end(), diff.begin(), 
                                           bind2nd(std::minus<float>(), mean));
  float sqSum = inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
  float variance = sqSum / (sample.size() - 1); // sample variance 

  float maxOv = 1.0;
  float maxCv = 1.0;
  float omega = segmentLen;

  float ov = min(omega/mean-1, maxOv);
  float cv = min(sqrt(variance)/mean, maxCv);

  
  if (DEBUG_ADAPTATION){	
    if (cv==maxCv)
      cout<<"*** cv exceeds "<<maxCv<<" ***\n";
    if (ov==maxOv)
      cout<<"*** ov exceeds "<<maxOv<<" ***\n";
  } 

  if (ov > 0)
    b = bMap[int(ov*100)][int(cv*100)];
  else
    b = 0;

  if (b == 0){
      while ((b == 0 || b > maxBuffSize - reservoirSize) && currentRepIdx > 0){
      float scale = float(reps.at(currentRepIdx-1)) / reps.at(currentRepIdx);
      --currentRepIdx;
      transform(sample.begin(), sample.end(), sample.begin(), 
                               bind2nd(std::multiplies<float>(), scale));
      mean*=scale;
      ov = min(omega/(mean)-1,maxOv);
      if (ov > 0)
        b = bMap[int(ov*100)][int(cv*100)];
      else 
        b = 0;
      if (DEBUG_ADAPTATION){
        cout << "AdaptationManager: downwardAdaptation\n"; 
        cout<< "currentRepIdx: "<<currentRepIdx<<endl;
      }
    }
  } else {
    int tmp; 
    while(currentRepIdx < reps.size()-1){
      float scale = float(reps.at(currentRepIdx + 1)) / reps.at(currentRepIdx);
      ov = min(omega/(mean*scale)-1,maxOv);
      if (ov > 0)
        tmp = bMap[int(ov*100)][int(cv*100)];
      else
        tmp = 0;
      if (tmp != 0 && tmp < int((1.0-beta) * float(maxBuffSize - reservoirSize))){ 
        transform(sample.begin(), sample.end(), sample.begin(), 
                                   bind2nd(std::multiplies<float>(), scale));
        mean*=scale;
        ++currentRepIdx; 
        b = tmp;
        if (DEBUG_ADAPTATION){	  
          cout<<"b "<<b<<endl;
          cout << "AdaptationManager: upwardAdaptation\n";	
          cout<< "currentRepIdx: "<<currentRepIdx<<endl;  
        }
      }else
        break;  
    }
  }
  if (DEBUG_ADAPTATION){	  
    cout<< "Finished adaptation\n"; 
    cout<< "currentRepIdx: "<< currentRepIdx<<endl;
    cout<< "b: "<< b << endl;
  }
}

uint64_t AdaptationManagerABMAplus::getBuffLen(){
  return reservoirLen + b * segmentLen;
}

