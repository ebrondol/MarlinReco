/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#include "FortranProcessor.h"
#include <iostream>

#ifdef MARLIN_USE_AIDA
#include <marlin/AIDAProcessor.h>
#include <AIDA/IHistogramFactory.h>
#include <AIDA/ICloud1D.h>
//#include <AIDA/IHistogram1D.h>
#endif
#include <EVENT/LCCollection.h>
#include <IMPL/LCCollectionVec.h>
#include <EVENT/MCParticle.h>
#include <EVENT/Track.h>
#include <IMPL/TrackImpl.h>
#include <IMPL/LCRelationImpl.h>
#include <IMPL/LCFlagImpl.h>

#include <cfortran.h>

#include"tpchitbank.h"
#include"tkhitbank.h"
#include"tktebank.h"
#include"tpc.h"
#include"marlin_tpcgeom.h"
#include"constants.h"

// STUFF needed for GEAR
#include <marlin/Global.h>
#include <gear/GEAR.h>
#include <gear/TPCParameters.h>
#include <gear/PadRowLayout2D.h>
//

PROTOCCALLSFFUN0(INT,TPCRUN,tpcrun)
#define TPCRUN() CCALLSFFUN0(TPCRUN,tpcrun)

  // FIXME:SJA: the namespace should be used explicitly
using namespace lcio ;
using namespace marlin ;
using namespace constants;

int writetpccpp(float c, int a, int b); 

FCALLSCFUN3(INT,writetpccpp,WRITETPCCPP,writetpccpp, FLOAT, INT, INT)

float readtpchitscpp(int a, int b); 

FCALLSCFUN2(FLOAT,readtpchitscpp,READTPCHITSCPP,readtpchitscpp, INT, INT)

int writetkhitcpp(float c, int a, int b); 

FCALLSCFUN3(INT,writetkhitcpp,WRITETKHITCPP,writetkhitcpp, FLOAT, INT, INT)

float readtkhitscpp(int a, int b); 

FCALLSCFUN2(FLOAT,readtkhitscpp,READTKHITSCPP,readtkhitscpp, INT, INT)

int writetktecpp(float c, int a, int b); 

FCALLSCFUN3(INT,writetktecpp,WRITETKTECPP,writetktecpp, FLOAT, INT, INT)

float readtktecpp(int a, int b); 

FCALLSCFUN2(FLOAT,readtktecpp,READTKTECPP,readtktecpp, INT, INT)

int tkmktecpp(int subid,int submod,int unused,int MesrCode,int PnteTE,int Q,int ndf,float chi2,float L,float cord1,float cord2,float cord3,float theta,float phi,float invp,float dedx,float cov[15]);
  

FCALLSCFUN17(INT,tkmktecpp,TKMKTECPP,tkmktecpp, INT , INT ,INT ,INT ,INT ,INT ,INT ,FLOAT ,FLOAT ,FLOAT ,FLOAT ,FLOAT ,FLOAT ,FLOAT ,FLOAT ,FLOAT ,FLOATV)

  int addhittktecpp(int a, int b); 

FCALLSCFUN2(INT,addhittktecpp,ADDHITTKTECPP,addhittktecpp, INT, INT)

FortranProcessor aFortranProcessor ;

FortranProcessor::FortranProcessor() : Processor("FortranProcessor") {
  
  // modify processor description
  _description = "Produces Track collection from TPC TrackerHit collections using LEP tracking algorithms" ;

  // register steering parameters: name, description, class-variable, default value

  registerProcessorParameter( "TPCTrackerHitCollectionName" , 
			      "Name of the TPC TrackerHit collection"  ,
			      _colNameTPC ,
			      std::string("TPCTrackerHits") ) ;

  registerProcessorParameter( "VTXTrackerHitCollectionName" , 
			      "Name of the VTX TrackerHit collection"  ,
			      _colNameVTX ,
			      std::string("VTXTrackerHits") ) ;

}


void FortranProcessor::init() { 

  // usually a good idea to
  printParameters() ;

  _nRun = 0 ;
  _nEvt = 0 ;
  
}

void FortranProcessor::processRunHeader( LCRunHeader* run) { 

  _nRun++ ;
} 

void FortranProcessor::processEvent( LCEvent * evt ) { 

  static bool firstEvent = true ;
    
  // this gets called for every event 
  // usually the working horse ...

  if(firstEvent==true) std::cout << "FortranProcessor called for first event" << std::endl;

  firstEvent = false ;
  
  LCCollection* tpcTHcol = evt->getCollection( _colNameTPC ) ;
  LCCollection* vtxTHcol = evt->getCollection( _colNameVTX ) ;
  
  const gear::TPCParameters& gearTPC = Global::GEAR->getTPCParameters() ;


  if( tpcTHcol != 0 ){

    LCCollectionVec* tpcTrackVec = new LCCollectionVec( LCIO::TRACK )  ;
    // if we want to point back to the hits we need to set the flag
    LCFlagImpl trkFlag(0) ;
    trkFlag.setBit( LCIO::TRBIT_HITS ) ;
    tpcTrackVec->setFlag( trkFlag.getFlag()  ) ;

    LCCollectionVec* lcRelVec = new LCCollectionVec( LCIO::LCRELATION )  ;

    int nTPCHits = tpcTHcol->getNumberOfElements()  ;   
        
    for(int i=0; i< nTPCHits; i++){
      
      TrackerHit* trkHitTPC = dynamic_cast<TrackerHit*>( tpcTHcol->getElementAt( i ) ) ;
      
      double *pos;
      float  de_dx;
      float  time;
  
      //      cellId = 	trkHitTPC->getCellID();
      pos = (double*) trkHitTPC->getPosition(); 
      de_dx = trkHitTPC->getdEdx();
      time = trkHitTPC->getTime();
      
      // convert to cm needed for BRAHMS(GEANT)
      float x = 0.1*pos[0];
      float y = 0.1*pos[1];
      float z = 0.1*pos[2];
      
      // convert de/dx from GeV (LCIO) to number of electrons 
      
      double tpcIonisationPotential = gearTPC.getDoubleVal("tpcIonPotential");
      de_dx = de_dx/tpcIonisationPotential;

      double tpcRPhiResMax = 0.1 * gearTPC.getDoubleVal("tpcRPhiResMax");
      double tpcRPhiRes = 0.1 * tpcRPhiResMax-fabs(pos[2])/gearTPC.getMaxDriftLength()*0.10;
      double tpcZRes = 0.1 * gearTPC.getDoubleVal("tpcZRes");


      // Brahms resolution code for TPC = 3 REF tkhtpc.F
      int icode = 3;
      int subid = 500;

      int mctrack = 0;
      
      TkHitBank->add_hit(x,y,z,de_dx,subid,mctrack,0,0,icode,tpcRPhiRes,tpcZRes);


      TPCHitBank->add_hit(x,y,z,de_dx,subid,tpcRPhiRes,tpcZRes,mctrack);

    } 
    
    cout << "the number of tpc hits sent to brahms = " << TPCHitBank->size() << endl;
    CNTPC.ntphits = TPCHitBank->size();
 
    //_____________________________________________________________________

    int nVTXHits = vtxTHcol->getNumberOfElements()  ;   
    
    for(int i=0; i< nVTXHits; i++){
      
      TrackerHit* trkHitVTX = dynamic_cast<TrackerHit*>( vtxTHcol->getElementAt( i ) ) ;
      
      double *pos;
      float  de_dx;
      float  time;
  
      //      cellId = 	trkHitVTX->getCellID();
      pos = (double*) trkHitVTX->getPosition(); 
      de_dx = trkHitVTX->getdEdx() ;
      time = trkHitVTX->getTime() ;
      
      // convert to cm needed for BRAHMS(GEANT)
      float x = 0.1*pos[0] ;
      float y = 0.1*pos[1] ;
      float z = 0.1*pos[2] ;
      
      
      // Brahms resolution code for VTX = 3 REF tkhtpc.F

      int subid = trkHitVTX->getType() ;
      
      // brsimu/brgeom/brtrac/code_f/vxpgeom.F:      VXDPPNT=7.0E-4
      float vtxRes = 0.0007 ;
      int resCode = 3 ;
      
      int mctrack = 0 ;
      
      TkHitBank->add_hit(x,y,z,de_dx,subid,mctrack,0,0,resCode,vtxRes,vtxRes) ;



    }

    //_____________________________________________________________________

    int err = TPCRUN();


    std::cout << "TPCRUN returns:" << err << std::endl;
    if(err!=0) std::cout << "have you set the ionisation potential correctly in the gear xml file" << std::endl;    
    
    for(int te=0; te<TkTeBank->size();te++){

      TrackImpl* tpcTrack = new TrackImpl ; 

      const double ref_r = 10.*TkTeBank->getCoord1_of_ref_point(te);
      const double ref_phi =TkTeBank->getCoord2_of_ref_point(te)/TkTeBank->getCoord1_of_ref_point(te);
      const double ref_z = 10.*TkTeBank->getCoord3_of_ref_point(te);

      //FIXME:SJA: B-field hard coded needs to redeemed
      // transformation from 1/p to 1/R = consb * (1/p) / sin(theta)
      // consb is given by 1/R = (c*B)/(pt*10^9) where B is in T and pt in GeV  

      const double consb = (2.99792458*4.)/(10*1000.);    // divide by 1000 m->mm

      // computation of D0 and Z0 taken from fkrtpe.F in Brahms
       
      // x0 and y0 of ref point 
      const double x0 = ref_r*cos(ref_phi);
      const double y0 = ref_r*sin(ref_phi);
      
      // signed track radius
      const double trkRadius = sin(TkTeBank->getTheta(te))/(consb*TkTeBank->getInvp(te));

      //      cout << "TkTeBank->getInvp(te) " << TkTeBank->getInvp(te) << endl;
      //      cout << " trkRadius = " << trkRadius << endl;
      
      // center of circumference
      const double xc = x0 - trkRadius * sin(TkTeBank->getPhi(te));
      const double yc = y0 + trkRadius * cos(TkTeBank->getPhi(te));
      
      // sign of geometric curvature: anti-clockwise == postive
      const double geom_curvature = fabs(TkTeBank->getInvp(te))/TkTeBank->getInvp(te);

      //      cout << "geom_curvature = " << geom_curvature << endl;
      
      const double xc2 = xc * xc;
      const double yc2 = yc * yc;
      
      // Set D0
      tpcTrack->setD0( trkRadius - geom_curvature * sqrt(xc2+yc2));
 
      // Phi at D0
      double phiatD0 = atan2(yc,xc)+(twopi/2.)+geom_curvature*(twopi/4.);
      if (phiatD0<0.) phiatD0 = phiatD0 + twopi;
      if (phiatD0>twopi) phiatD0 = phiatD0 - twopi;

      //      cout << "phi at ref = " << ref_phi << endl;
      //      cout << "phi at D0 = " << phiatD0 << endl;
 
      // difference between phi at ref and D0 
      const double dphi = fmod((phiatD0 - TkTeBank->getPhi(te) +  twopi + (twopi/2.) ) , twopi ) - (twopi/2.);
      
      //      cout << "dphi at D0 = " << dphi << endl;

      // signed length of arc
      const double larc = trkRadius * dphi;

      // Set Z0
      tpcTrack->setZ0(ref_z+(1/(tan(TkTeBank->getTheta(te)))*larc)); 
      
      //      cout << " D0 = " << trkRadius - geom_curvature * sqrt(xc2+yc2) << endl;
      //      cout << " Z0 = " << ref_z+1/(tan(TkTeBank->getTheta(te)))*larc  << endl;

      //FIXME: SJA: phi is set at PCA not ref this should be resolved
      //      tpcTrack->setPhi(TkTeBank->getPhi(te));
      tpcTrack->setPhi(phiatD0);

      // tan lambda and curvature remain unchanged as the track is only extrapolated
      // set negative as 1/p is signed with geometric curvature clockwise negative
      tpcTrack->setOmega((-consb*TkTeBank->getInvp(te))/sin(TkTeBank->getTheta(te)));
      tpcTrack->setTanLambda(tan((twopi/4.)-TkTeBank->getTheta(te)));

      tpcTrack->setIsReferencePointPCA(false);
      tpcTrack->setChi2(TkTeBank->getChi2(te));
      tpcTrack->setNdf(TkTeBank->getNdf(te));
      tpcTrack->setdEdx(TkTeBank->getDe_dx(te));
     

      const vector <int> * hits;
      vector<MCParticle*> mcPointers;
      vector<int> mcHits;

      hits = TkTeBank->getHitlist(te);

      for(unsigned int tehit=0; tehit<hits->size();tehit++){
	TrackerHit* trkHitTPC = dynamic_cast<TrackerHit*>( tpcTHcol->getElementAt( hits->at(tehit) ) ) ;

	tpcTrack->addHit(trkHitTPC);

	for(unsigned int j=0; j<trkHitTPC->getRawHits().size(); j++){ 
	  
	  SimTrackerHit * simTrkHitTPC =dynamic_cast<SimTrackerHit*>(trkHitTPC->getRawHits().at(j));
	  MCParticle * mcp = dynamic_cast<MCParticle*>(simTrkHitTPC->getMCParticle()); 
	  if(mcp == NULL) cout << "mc particle pointer = null" << endl; 
	  
	  bool found = false;
	  
	  for(unsigned int k=0; k<mcPointers.size();k++)
	    {
	      if(mcp==mcPointers[k]){
		found=true;
		mcHits[k]++;
	      }
	    }
	  if(!found){
	    mcPointers.push_back(mcp);
	    mcHits.push_back(1);
	  }
	}
      }
      
      for(unsigned int k=0; k<mcPointers.size();k++){

	MCParticle * mcp = mcPointers[k];
	
	LCRelationImpl* lcRel = new LCRelationImpl;
	lcRel->setFrom (tpcTrack);
	lcRel->setTo (mcp);
	float weight = mcHits[k]/tpcTrack->getTrackerHits().size();
	lcRel->setWeight(weight);
	
	lcRelVec->addElement( lcRel );
      }
            
      //FIXME:SJA:  Covariance matrix not included yet needs converting for 1/R and TanLambda
      

      tpcTrackVec->addElement( tpcTrack );

      
//      std::cout << "TkTeBank->getSubdetector_ID(te) = " << TkTeBank->getSubdetector_ID(te) << std::endl;
//      std::cout << "TkTeBank->getSubmodule(te) = " << TkTeBank->getSubmodule(te) << std::endl;
//      std::cout << "TkTeBank->getUnused(te) = " << TkTeBank->getUnused(te) << std::endl;
//      std::cout << "TkTeBank->getMeasurement_code(te) = " << TkTeBank->getMeasurement_code(te) << std::endl;
//      std::cout << "TkTeBank->getPointer_to_end_of_TE(te) = " << TkTeBank->getPointer_to_end_of_TE(te) << std::endl;
//      std::cout << "TkTeBank->getNdf(te) = " << TkTeBank->getNdf(te) << std::endl;
//      std::cout << "TkTeBank->getChi2(te) = " << TkTeBank->getChi2(te) << std::endl;
//      std::cout << "TkTeBank->getLength(te) = " << TkTeBank->getLength(te) << std::endl;
//      std::cout << "TkTeBank->getCoord1_of_ref_point(te) = " << TkTeBank->getCoord1_of_ref_point(te) << std::endl;
//      std::cout << "TkTeBank->getCoord2_of_ref_point(te) = " << TkTeBank->getCoord2_of_ref_point(te) << std::endl;
//      std::cout << "TkTeBank->getCoord3_of_ref_point(te) = " << TkTeBank->getCoord3_of_ref_point(te) << std::endl;
//      std::cout << "TkTeBank->getTheta(te) = " << TkTeBank->getTheta(te) << std::endl;
//      std::cout << "TkTeBank->getPhi(te) = " << TkTeBank->getPhi(te) << std::endl;
//      std::cout << "TkTeBank->getInvp(te) = " << TkTeBank->getInvp(te) << std::endl;
//      std::cout << "TkTeBank->getDe_dx(te) = " << TkTeBank->getDe_dx(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix1(te) = " << TkTeBank->getCovmatrix1(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix2(te) = " << TkTeBank->getCovmatrix2(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix3(te) = " << TkTeBank->getCovmatrix3(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix4(te) = " << TkTeBank->getCovmatrix4(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix5(te) = " << TkTeBank->getCovmatrix5(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix6(te) = " << TkTeBank->getCovmatrix6(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix7(te) = " << TkTeBank->getCovmatrix7(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix8(te) = " << TkTeBank->getCovmatrix8(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix9(te) = " << TkTeBank->getCovmatrix9(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix0(te) = " << TkTeBank->getCovmatrix10(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix1(te) = " << TkTeBank->getCovmatrix11(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix2(te) = " << TkTeBank->getCovmatrix12(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix3(te) = " << TkTeBank->getCovmatrix13(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix4(te) = " << TkTeBank->getCovmatrix14(te) << std::endl;
//      std::cout << "TkTeBank->getCovmatrix5(te) = " << TkTeBank->getCovmatrix15(te) << std::endl;
//
    }

    // set the parameters to decode the type information in the collection
    // for the time being this has to be done manually
    // in the future we should provide a more convenient mechanism to 
    // decode this sort of meta information

//     StringVec typeNames ;
//     IntVec typeValues ;
//     typeNames.push_back( LCIO::TRACK ) ;
//     typeValues.push_back( 1 ) ;
//     tpcTrackVec->parameters().setValues("TrackTypeNames" , typeNames ) ;
//     tpcTrackVec->parameters().setValues("TrackTypeValues" , typeValues ) ;
    
    evt->addCollection( tpcTrackVec , "TPCTracks") ;
    evt->addCollection( lcRelVec , "MCTrackRelations") ;
    

  }
  
  _nEvt ++ ;
  
}



void FortranProcessor::check( LCEvent * evt ) { 
  // nothing to check here - could be used to fill checkplots in reconstruction processor
}


void FortranProcessor::end(){ 
  
//   std::cout << "FortranProcessor::end()  " << name() 
// 	    << " processed " << _nEvt << " events in " << _nRun << " runs "
// 	    << std::endl ;

}

