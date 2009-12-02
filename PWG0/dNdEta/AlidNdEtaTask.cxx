/* $Id$ */

#include "AlidNdEtaTask.h"

#include <TStyle.h>
#include <TSystem.h>
#include <TCanvas.h>
#include <TVector3.h>
#include <TChain.h>
#include <TFile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TH3F.h>
#include <TParticle.h>
#include <TRandom.h>
#include <TNtuple.h>
#include <TObjString.h>
#include <TF1.h>
#include <TGraph.h>

#include <AliLog.h>
#include <AliESDVertex.h>
#include <AliESDEvent.h>
#include <AliStack.h>
#include <AliHeader.h>
#include <AliGenEventHeader.h>
#include <AliMultiplicity.h>
#include <AliAnalysisManager.h>
#include <AliMCEventHandler.h>
#include <AliMCEvent.h>
#include <AliESDInputHandler.h>
#include <AliESDInputHandlerRP.h>
#include <AliESDHeader.h>

#include "AliESDtrackCuts.h"
#include "AliPWG0Helper.h"
#include "AliCorrection.h"
#include "AliCorrectionMatrix3D.h"
#include "dNdEta/dNdEtaAnalysis.h"
#include "AliTriggerAnalysis.h"

ClassImp(AlidNdEtaTask)

AlidNdEtaTask::AlidNdEtaTask(const char* opt) :
  AliAnalysisTask("AlidNdEtaTask", ""),
  fESD(0),
  fOutput(0),
  fOption(opt),
  fAnalysisMode((AliPWG0Helper::AnalysisMode) (AliPWG0Helper::kTPC | AliPWG0Helper::kFieldOn)),
  fTrigger(AliTriggerAnalysis::kMB1),
  fFillPhi(kFALSE),
  fDeltaPhiCut(-1),
  fReadMC(kFALSE),
  fUseMCVertex(kFALSE),
  fOnlyPrimaries(kFALSE),
  fUseMCKine(kFALSE),
  fCheckEventType(kFALSE),
  fSymmetrize(kFALSE),
  fEsdTrackCuts(0),
  fdNdEtaAnalysisESD(0),
  fMult(0),
  fMultVtx(0),
  fEvents(0),
  fVertexResolution(0),
  fdNdEtaAnalysis(0),
  fdNdEtaAnalysisND(0),
  fdNdEtaAnalysisNSD(0),
  fdNdEtaAnalysisTr(0),
  fdNdEtaAnalysisTrVtx(0),
  fdNdEtaAnalysisTracks(0),
  fPartPt(0),
  fVertex(0),
  fVertexVsMult(0),
  fPhi(0),
  fRawPt(0),
  fEtaPhi(0),
  fDeltaPhi(0),
  fDeltaTheta(0),
  fFiredChips(0),
  fTrackletsVsClusters(0),
  fTrackletsVsUnassigned(0),
  fTriggerVsTime(0),
  fStats(0),
  fStats2(0)
{
  //
  // Constructor. Initialization of pointers
  //

  // Define input and output slots here
  DefineInput(0, TChain::Class());
  DefineOutput(0, TList::Class());
  
  fZPhi[0] = 0;
  fZPhi[1] = 0;

  AliLog::SetClassDebugLevel("AlidNdEtaTask", AliLog::kWarning);
}

AlidNdEtaTask::~AlidNdEtaTask()
{
  //
  // Destructor
  //

  // histograms are in the output list and deleted when the output
  // list is deleted by the TSelector dtor

  if (fOutput) {
    delete fOutput;
    fOutput = 0;
  }
}

Bool_t AlidNdEtaTask::Notify()
{
  static Int_t count = 0;
  count++;
  Printf("Processing %d. file: %s", count, ((TTree*) GetInputData(0))->GetCurrentFile()->GetName());
  return kTRUE;
}

//________________________________________________________________________
void AlidNdEtaTask::ConnectInputData(Option_t *)
{
  // Connect ESD
  // Called once

  Printf("AlidNdEtaTask::ConnectInputData called");

  AliESDInputHandler *esdH = dynamic_cast<AliESDInputHandler*> (AliAnalysisManager::GetAnalysisManager()->GetInputEventHandler());

  if (!esdH) {
    Printf("ERROR: Could not get ESDInputHandler");
  } else {
    fESD = esdH->GetEvent();
    
    TString branches("AliESDHeader Vertex ");

    if (fAnalysisMode & AliPWG0Helper::kSPD || fTrigger & AliTriggerAnalysis::kOfflineFlag)
      branches += "AliMultiplicity ";
      
    if (fAnalysisMode & AliPWG0Helper::kTPC || fAnalysisMode & AliPWG0Helper::kTPCITS)
      branches += "Tracks ";
  
    // Enable only the needed branches
    esdH->SetActiveBranches(branches);
  }

  // disable info messages of AliMCEvent (per event)
  AliLog::SetClassDebugLevel("AliMCEvent", AliLog::kWarning - AliLog::kDebug + 1);
}

void AlidNdEtaTask::CreateOutputObjects()
{
  // create result objects and add to output list

  Printf("AlidNdEtaTask::CreateOutputObjects");

  if (fOnlyPrimaries)
    Printf("WARNING: Processing only primaries (MC information used). This is for systematical checks only.");

  if (fUseMCKine)
    Printf("WARNING: Using MC kine information. This is for systematical checks only.");

  if (fUseMCVertex)
    Printf("WARNING: Replacing vertex by MC vertex. This is for systematical checks only.");

  fOutput = new TList;
  fOutput->SetOwner();

  fdNdEtaAnalysisESD = new dNdEtaAnalysis("fdNdEtaAnalysisESD", "fdNdEtaAnalysisESD", fAnalysisMode);
  fOutput->Add(fdNdEtaAnalysisESD);

  fMult = new TH1F("fMult", "fMult;Ntracks;Count", 201, -0.5, 200.5);
  fOutput->Add(fMult);

  fMultVtx = new TH1F("fMultVtx", "fMultVtx;Ntracks;Count", 201, -0.5, 200.5);
  fOutput->Add(fMultVtx);

  for (Int_t i=0; i<3; ++i)
  {
    fPartEta[i] = new TH1F(Form("dndeta_check_%d", i), Form("dndeta_check_%d", i), 60, -6, 6);
    fPartEta[i]->Sumw2();
    fOutput->Add(fPartEta[i]);
  }

  fEvents = new TH1F("dndeta_check_vertex", "dndeta_check_vertex", 800, -40, 40);
  fOutput->Add(fEvents);

  Float_t resMax = (fAnalysisMode & AliPWG0Helper::kSPD) ? 0.2 : 2;
  fVertexResolution = new TH1F("dndeta_vertex_resolution_z", "dndeta_vertex_resolution_z", 1000, 0, resMax);
  fOutput->Add(fVertexResolution);

  fPhi = new TH1F("fPhi", "fPhi;#phi in rad.;count", 720, 0, 2 * TMath::Pi());
  fOutput->Add(fPhi);

  fEtaPhi = new TH2F("fEtaPhi", "fEtaPhi;#eta;#phi in rad.;count", 80, -4, 4, 18*5, 0, 2 * TMath::Pi());
  fOutput->Add(fEtaPhi);

  fTriggerVsTime = new TGraph; //TH1F("fTriggerVsTime", "fTriggerVsTime;trigger time;count", 100, 0, 100);
  fTriggerVsTime->SetName("fTriggerVsTime");
  fTriggerVsTime->GetXaxis()->SetTitle("trigger time");
  fTriggerVsTime->GetYaxis()->SetTitle("count");
  fOutput->Add(fTriggerVsTime);

  fStats = new TH1F("fStats", "fStats", 5, 0.5, 5.5);
  fStats->GetXaxis()->SetBinLabel(1, "vertexer 3d");
  fStats->GetXaxis()->SetBinLabel(2, "vertexer z");
  fStats->GetXaxis()->SetBinLabel(3, "trigger");
  fStats->GetXaxis()->SetBinLabel(4, "physics events");
  fStats->GetXaxis()->SetBinLabel(5, "physics events after veto");
  fOutput->Add(fStats);
  
  fStats2 = new TH2F("fStats2", "fStats2", 6, -0.5, 5.5, 7, -0.5, 6.5);
  fStats2->GetXaxis()->SetBinLabel(1, "No Vertex");
  fStats2->GetXaxis()->SetBinLabel(2, "|z-vtx| > 10");
  fStats2->GetXaxis()->SetBinLabel(3, "0 tracklets");
  fStats2->GetXaxis()->SetBinLabel(4, "Splash identification");
  fStats2->GetXaxis()->SetBinLabel(5, "Scan Veto");
  fStats2->GetXaxis()->SetBinLabel(6, "Selected");
  fStats2->GetYaxis()->SetBinLabel(1, "n/a");
  fStats2->GetYaxis()->SetBinLabel(2, "emptyA");
  fStats2->GetYaxis()->SetBinLabel(3, "emptyC");
  fStats2->GetYaxis()->SetBinLabel(4, "emptyAC");
  fStats2->GetYaxis()->SetBinLabel(5, "BB");
  fStats2->GetYaxis()->SetBinLabel(6, "BGA");
  fStats2->GetYaxis()->SetBinLabel(7, "BGC");
  fOutput->Add(fStats2);

  if (fAnalysisMode & AliPWG0Helper::kSPD)
  {
    fDeltaPhi = new TH1F("fDeltaPhi", "fDeltaPhi;#Delta #phi;Entries", 500, -0.2, 0.2);
    fOutput->Add(fDeltaPhi);
    fDeltaTheta = new TH1F("fDeltaTheta", "fDeltaTheta;#Delta #theta;Entries", 500, -0.2, 0.2);
    fOutput->Add(fDeltaTheta);
    fFiredChips = new TH2F("fFiredChips", "fFiredChips;Chips L1 + L2;tracklets", 1201, -0.5, 1201, 50, -0.5, 49.5);
    fOutput->Add(fFiredChips);
    fTrackletsVsClusters = new TH2F("fTrackletsVsClusters", ";tracklets;clusters in ITS", 50, -0.5, 49.5, 1000, -0.5, 999.5);
    fOutput->Add(fTrackletsVsClusters);
    fTrackletsVsUnassigned = new TH2F("fTrackletsVsUnassigned", ";tracklets;unassigned clusters in L0", 50, -0.5, 49.5, 200, -0.5, 199.5);
    fOutput->Add(fTrackletsVsUnassigned);
    for (Int_t i=0; i<2; i++)
    {
      fZPhi[i] = new TH2F(Form("fZPhi_%d", i), Form("fZPhi Layer %d;z (cm);#phi (rad.)", i), 200, -20, 20, 180, 0, TMath::Pi() * 2);
      fOutput->Add(fZPhi[i]);
    }
  }

  if (fAnalysisMode & AliPWG0Helper::kTPC || fAnalysisMode & AliPWG0Helper::kTPCITS)
  {
    fRawPt =  new TH1F("fRawPt", "raw pt;p_{T};Count", 2000, 0, 100);
    fOutput->Add(fRawPt);
  }

  fVertex = new TH3F("vertex_check", "vertex_check;x;y;z", 100, -1, 1, 100, -1, 1, 100, -30, 30);
  fOutput->Add(fVertex);
  
  fVertexVsMult = new TH3F("fVertexVsMult", "fVertexVsMult;x;y;multiplicity", 100, -1, 1, 100, -1, 1, 100, -0.5, 99.5);
  fOutput->Add(fVertexVsMult);

  if (fReadMC)
  {
    fdNdEtaAnalysis = new dNdEtaAnalysis("dndeta", "dndeta", fAnalysisMode);
    fOutput->Add(fdNdEtaAnalysis);

    fdNdEtaAnalysisND = new dNdEtaAnalysis("dndetaND", "dndetaND", fAnalysisMode);
    fOutput->Add(fdNdEtaAnalysisND);

    fdNdEtaAnalysisNSD = new dNdEtaAnalysis("dndetaNSD", "dndetaNSD", fAnalysisMode);
    fOutput->Add(fdNdEtaAnalysisNSD);

    fdNdEtaAnalysisTr = new dNdEtaAnalysis("dndetaTr", "dndetaTr", fAnalysisMode);
    fOutput->Add(fdNdEtaAnalysisTr);

    fdNdEtaAnalysisTrVtx = new dNdEtaAnalysis("dndetaTrVtx", "dndetaTrVtx", fAnalysisMode);
    fOutput->Add(fdNdEtaAnalysisTrVtx);

    fdNdEtaAnalysisTracks = new dNdEtaAnalysis("dndetaTracks", "dndetaTracks", fAnalysisMode);
    fOutput->Add(fdNdEtaAnalysisTracks);

    fPartPt =  new TH1F("dndeta_check_pt", "dndeta_check_pt", 1000, 0, 10);
    fPartPt->Sumw2();
    fOutput->Add(fPartPt);
  }

  if (fEsdTrackCuts)
  {
    fEsdTrackCuts->SetName("fEsdTrackCuts");
    fOutput->Add(fEsdTrackCuts);
  }
}

void AlidNdEtaTask::Exec(Option_t*)
{
  // process the event

  // these variables are also used in the MC section below; however, if ESD is off they stay with their default values
  Bool_t eventTriggered = kFALSE;
  const AliESDVertex* vtxESD = 0;

  // post the data already here
  PostData(0, fOutput);

  // ESD analysis
  if (fESD)
  {
    AliESDHeader* esdHeader = fESD->GetHeader();
    if (!esdHeader)
    {
      Printf("ERROR: esdHeader could not be retrieved");
      return;
    }
    
    //Printf("Trigger classes: %s:", fESD->GetFiredTriggerClasses().Data());

    // check event type (should be PHYSICS = 7)
    if (fCheckEventType)
    {
      UInt_t eventType = esdHeader->GetEventType();
      if (eventType != 7)
      {
        Printf("Skipping event because it is of type %d", eventType);
        return;
      }
      
      fStats->Fill(4);
      
      const Int_t kMaxEvents = 30;
      UInt_t maskedEvents[kMaxEvents][2] = { 
      {0, 0x1ac4a5},
      {0, 0x4a34af}, 
      {0, 0x697b0f}, 
      {0, 0x74a07d}, 
      {0, 0x864339}, 
      {0, 0x94fbfb}, 
      {0, 0xb87d2b}, 
      {0, 0xd471f3}, 
      {0, 0xddb0e3}, 
      {0, 0xf30af3}, 
      {1, 0xef46f}, 
      {1, 0x12cdd1}, 
      {1, 0x2d7591}, 
      {1, 0x925245}, 
      {1, 0x9af0fd}, 
      {1, 0xb880d5}, 
      {1, 0xb91b0b}, 
      {0, 0x134066}, 
      {0, 0x140dbc}, 
      {0, 0x1d85f6}, 
      {0, 0x8cb1da}, 
      {0, 0xf9b31c}, 
      {1, 0x500dbc}, 
      {1, 0x587134}, 
      {1, 0x687dbe}, 
      {1, 0x6eab78}, 
      {1, 0x894caa},
      {0, 0x845fb3}, 
      {0, 0x22b966}, 
      {0, 0xb1d576}
   
//      {0, 0xbe8f99}, //put back in after discussion with adam
//      {1, 0x2b43d4}, //put back in after discussion with adam

/*      ,{0, 0x23585E}, // jet test
      {0, 0x50EF2F},
      {0, 0x672ADD}*/
      };
      
      Bool_t veto = kFALSE;

      for (Int_t i=0; i<kMaxEvents; i++)
      {
        if (fESD->GetPeriodNumber() == maskedEvents[i][0] && fESD->GetOrbitNumber() == maskedEvents[i][1])
        {
           Printf("Skipping event because it is masked: period: %d orbit: %x", fESD->GetPeriodNumber(), fESD->GetOrbitNumber());
           veto = kTRUE;
        }
      }
      
      Int_t decision = (veto) ? 4 : -1;
      
      if (!veto)
      {
        // ITS cluster tree
        AliESDInputHandlerRP* handlerRP = dynamic_cast<AliESDInputHandlerRP*> (AliAnalysisManager::GetAnalysisManager()->GetInputEventHandler());
        if (!handlerRP)
          return;
          
        TTree* itsClusterTree = handlerRP->GetTreeR("ITS");  
        if (!itsClusterTree)
          return;
          
        TClonesArray* itsClusters = new TClonesArray("AliITSRecPoint");
        TBranch* itsClusterBranch=itsClusterTree->GetBranch("ITSRecPoints");
      
        itsClusterBranch->SetAddress(&itsClusters);
      
        Int_t nItsSubs = (Int_t)itsClusterTree->GetEntries();  
      
        Int_t totalClusters = 0;
          
        // loop over the its subdetectors
        for (Int_t iIts=0; iIts < nItsSubs; iIts++) {
          
          if (!itsClusterTree->GetEvent(iIts)) 
            continue;
          
          Int_t nClusters = itsClusters->GetEntriesFast();
          totalClusters += nClusters;
        }
                
        const AliMultiplicity* mult = fESD->GetMultiplicity();
        if (!mult)
          return;
        
        fTrackletsVsClusters->Fill(mult->GetNumberOfTracklets(), totalClusters);
              
        Int_t limit = 80 + mult->GetNumberOfTracklets() * 220/20;
        
        if (totalClusters > limit)
        {
          Printf("Skipping event because %d clusters is above limit of %d from %d tracklets: Period number: %d Orbit number: %x", totalClusters, limit, mult->GetNumberOfTracklets(), fESD->GetPeriodNumber(), fESD->GetOrbitNumber());
          decision = 3;
        }
        else 
        {
          vtxESD = AliPWG0Helper::GetVertex(fESD, fAnalysisMode);
          if (!vtxESD)
          {
            decision = 0;
          }
          else
          {
            Double_t vtx[3];
            vtxESD->GetXYZ(vtx);
            
            if (TMath::Abs(vtx[2]) > 10)
            {
              decision = 1;
            }
            else
            {
              if (mult->GetNumberOfTracklets() == 0)
              {
                decision = 2;
              }
              else
              {
                decision = 5;
              }
            }
          }
        }
      }
      
      const Int_t kMaxVZero = 190;
      
      #define emptyA 1
      #define emptyC 2
      #define BBA 3
      #define BBC 4
      #define BGA 5
      #define BGC 6
      UInt_t vzeroAnalysis[kMaxVZero][4] = {
        
        {0, 0x5d31bc, emptyA, BBC},
        {0, 0x5e4dcc, BBA, BBC},
        {0, 0x6063a8, BBA, BBC},
        {0, 0x613d53, BBA, BBC},
        {0, 0x655069, BBA, BBC},
        {0, 0x65e1cf, BBA, BBC},
        {0, 0x66cea0, BBA, BBC},
        {0, 0x672add, BBA, BBC},
        {0, 0x68e567, BBA, BBC},
        {0, 0x697b0f, BBA, emptyC},
        {0, 0x6a686d, BGA, BBC},
        {0, 0x6c4c2f, BBA, BBC},
        {0, 0x6ca84d, BBA, BBC},
        {0, 0x6ee60b, BBA, BBC},
        {0, 0x6fa64f, BBA, BBC},
        {0, 0x71cc8d, BBA, BBC},
        {0, 0x74a07d, BBA, BBC},
        {0, 0x77761b, BGA, BBC},
        {0, 0x783454, BBA, BBC},
        {0, 0x7d2414, BBA, BBC},
        {0, 0x7f443f, BBA, BBC},
        {0, 0x815daa, BBA, BBC},
        {0, 0x8367dd, BBA, BBC},
        {0, 0x838852, BBA, BBC},
        {0, 0x845fb3, BBA, BBC},
        {0, 0x84c722, emptyA, BBC},
        {0, 0x86256e, BBA, BBC},
        {0, 0x864339, BBA, BBC},
        {0, 0x869be0, BGA, BBC},
        {0, 0x895222, BBA, BBC},
        {0, 0x8c1a56, emptyA, BBC},
        {0, 0x8cb1da, BBA, BBC},
        {0, 0x8e954e, BBA, BGC},
        {0, 0x8fe645, BBA, BBC},
        {0, 0x91bafc, BBA, BBC},
        {0, 0x93ae80, BBA, BBC},
        {0, 0x94f793, BBA, BBC},
        {0, 0x94fbfb, BBA, BBC},
        {0, 0x954c20, emptyA, BBC},
        {0, 0x965e0f, BBA, BBC},
        {0, 0x96a30e, BBA, BBC},
        {0, 0x972f1d, BBA, BBC},
        {0, 0x98fd10, BBA, BBC},
        {0, 0x9a0472, BBA, BBC},
        {0, 0x9c4894, BBA, BBC},
        {0, 0x9e6dc8, emptyA, emptyC},
        {0, 0x9f5e8b, BBA, BBC},
        {0, 0xa035bb, BBA, BGC},
        {0, 0xa53f7c, BBA, BBC},
        {0, 0xa56537, BBA, BBC},
        {0, 0xa60957, BBA, BBC},
        {0, 0xa689db, BBA, BBC},
        {0, 0xa6d494, BBA, BBC},
        {0, 0xa6d935, BBA, BBC},
        {0, 0xa8671a, BBA, BBC},
        {0, 0xaa7956, BBA, BBC},
        {0, 0xaee392, BBA, BBC},
        {0, 0xb1d576, BBA, BBC},
        {0, 0xb235c3, BBA, BGC},
        {0, 0xb24b2d, BBA, BBC},
        {0, 0xb2f999, BBA, emptyC},
        {0, 0xb5acac, BBA, BBC},
        {0, 0xb60ef4, BBA, BBC},
        {0, 0xb7f38a, BBA, BBC},
        {0, 0xb87d2b, BBA, BBC},
        {0, 0xb8d564, BGA, BBC},
        {0, 0xb9722e, BBA, BBC},
        {0, 0xb98a5b, BBA, BBC},
        {0, 0xb99f55, BBA, BBC},
        {0, 0xbda770, BBA, BBC},
        {0, 0xbe8f99, BBA, BBC},
        {0, 0xc226a0, emptyA, BBC},
        {0, 0xc2b248, BBA, BBC},
        {0, 0xc56f42, emptyA, emptyC},
        {0, 0xc6ec01, BBA, emptyC},
        {0, 0xc768f2, BBA, BBC},
        {0, 0xc7f149, BBA, BBC},
        {0, 0xc94a96, BBA, BBC},
        {0, 0xcbf834, BBA, BBC},
        {0, 0xce4b94, BBA, BBC},
        {0, 0xcf1b7b, BBA, BBC},
        {0, 0xcf7257, BBA, BBC},
        {0, 0xcffe10, BBA, BBC},
        {0, 0xd471f3, BBA, BBC},
        {0, 0xd5b0c6, BBA, BGC},
        {0, 0xd5c7e7, BBA, BBC},
        {0, 0xd8b421, emptyA, emptyC},
        {0, 0xd978ed, BBA, BBC},
        {0, 0xdbee52, BBA, emptyC},
        {0, 0xdc0425, BBA, BBC},
        {0, 0xdcc9e1, BBA, BBC},
        {0, 0xddb0e3, BBA, BBC},
        {0, 0xddd1a7, BGA, BBC},
        {0, 0xdfd2c6, BBA, BBC},
        {0, 0xe1db66, BBA, BBC},
        {0, 0xe27106, BBA, BBC},
        {0, 0xe69c79, BBA, BBC},
        {0, 0xe71d7b, BBA, BBC},
        {0, 0xe80a68, BBA, BBC},
        {0, 0xea06fc, BBA, BBC},
        {0, 0xf01c8e, BBA, BBC},
        {0, 0xf08660, BBA, BBC},
        {0, 0xf1a165, BBA, BBC},
        {0, 0xf30af3, BBA, BBC},
        {0, 0xf7635b, BGA, BBC},
        {0, 0xf7a36c, emptyA, BBC},
        {0, 0xf9b31c, BBA, BBC},
        {0, 0xfb35f9, emptyA, BBC},
        {0, 0xfc22dd, BBA, BBC},
        {0, 0xfe58ba, BBA, BBC},
        {0, 0xff5c1f, BBA, BBC},
        {0, 0xff93b5, emptyA, BBC},
        {1, 0x3513, BBA, BBC},
        {1, 0x20656, BBA, BBC},
        {1, 0x4345d, BBA, BBC},
        {1, 0x5ec2f, BBA, BBC},
        {1, 0x8140f, BBA, BBC},
        {1, 0x93898, BBA, BBC},
        {1, 0xef46f, BBA, BBC},
        {1, 0xfff01, BGA, BBC},
        {1, 0x10ce3e, BBA, BBC},
        {1, 0x117f6a, BBA, BBC},
        {1, 0x12cdd1, BBA, emptyC},
        {1, 0x14b973, BBA, BGC},
        {1, 0x155e84, BBA, BBC},
        {1, 0x181dda, BBA, BBC},
        {1, 0x1bd5b5, BBA, BBC},
        {1, 0x1ca304, BBA, BBC},
        {1, 0x1d095a, emptyA, BBC},
        {1, 0x1e0125, BBA, BBC},
        {1, 0x24a1c5, BBA, BBC},
        {1, 0x24c960, BBA, BBC},
        {1, 0x2638f4, BBA, BBC},
        {1, 0x27cdee, BBA, BBC},
        {1, 0x283e0d, BBA, BBC},
        {1, 0x29b1f3, BBA, BBC},
        {1, 0x2b43d4, BBA, BBC},
        {1, 0x2bb918, BBA, BBC},
        {1, 0x2d7591, BBA, BBC},
        {1, 0x2da5ac, BGA, BBC},
        {1, 0x2e2d65, BBA, BBC},
        {1, 0x33bef9, BBA, BBC},
        {1, 0x35505d, BBA, BBC},
        {1, 0x36084d, BBA, BBC},
        {1, 0x380b1f, emptyA, emptyC},
        {1, 0x38d478, BBA, BBC},
        {1, 0x3a0622, BBA, BBC},
        {1, 0x3a194e, emptyA, emptyC},
        {1, 0x3b5972, emptyA, BBC},
        {1, 0x3ed6f5, BBA, BBC},
        {1, 0x3ef093, BBA, BBC},
        {1, 0x422847, BBA, BBC},
        {1, 0x426bbb, BBA, BBC},
        {1, 0x4b2f9b, BBA, BBC},
        {1, 0x4c5781, emptyA, BBC},
        {1, 0x4f1137, BBA, BBC},
        {1, 0x4fe5ae, BBA, BBC},
        {1, 0x500dbc, BBA, BBC},
        {1, 0x502a5e, BGA, BBC},
        {1, 0x505769, BBA, BBC},
        {1, 0x507bd3, BBA, BBC},
        {1, 0x55b3ef, BBA, BBC},
        {1, 0x56333f, BBA, BBC},
        {1, 0x587134, BBA, BBC},
        {1, 0x5b4847, BBA, BGC},
        {1, 0x5b777a, BBA, BBC},
        {1, 0x5b7dde, BBA, BBC},
        {1, 0x5b7e14, BBA, BBC},
        {1, 0x5cd9ca, BBA, BBC},
        {1, 0x5e7c42, BBA, BBC},
        {1, 0x5facec, BBA, BBC},
        {1, 0x6030ae, BBA, BBC},
        {1, 0x64a772, BBA, BBC},
        {1, 0x687dbe, BBA, BBC},
        {1, 0x68c5dd, BBA, BGC},
        {1, 0x692064, BBA, BBC},
        {1, 0x6949da, BBA, BBC},
        {1, 0x6db110, BBA, BBC},
        {1, 0x6eab78, BBA, BBC},
        {1, 0x6fc13d, BGA, BBC},
        {1, 0x7463b6, BBA, BBC},
        {1, 0x749cec, BBA, BBC},
        {1, 0x756547, BBA, BBC},
        {1, 0x77819e, BBA, BBC},
        {1, 0x785e0b, BBA, BBC},
        {1, 0x7b3caa, BBA, BBC},
        {1, 0x7cccbb, BBA, BBC},
        {1, 0x7e7e17, BBA, BBC},
        {1, 0x7fc0f5, BBA, BBC},
        {1, 0x806ee5, BBA, emptyC}
        
/* first cvetan list         
        {0, 0x5d31bc, emptyA, emptyC},
        {0, 0x5e4dcc, BBA, BBC},
        {0, 0x6063a8, BBA, BBC},
        {0, 0x613d53, BBA, BBC},
        {0, 0x655069, BBA, BBC},
        {0, 0x65e1cf, BBA, BBC},
        {0, 0x66cea0, BBA, BBC},
        {0, 0x672add, BBA, BBC},
        {0, 0x68e567, BBA, BBC},
        {0, 0x697b0f, emptyA, emptyC},
        {0, 0x6a686d, BGA, BBC},
        {0, 0x6c4c2f, BBA, BBC},
        {0, 0x6ca84d, BBA, BBC},
        {0, 0x6ee60b, BBA, BBC},
        {0, 0x6fa64f, BBA, BBC},
        {0, 0x71cc8d, BBA, BBC},
        {0, 0x74a07d, BBA, BBC},
        {0, 0x77761b, BGA, BBC},
        {0, 0x783454, BBA, BBC},
        {0, 0x7d2414, BBA, BBC},
        {0, 0x7f443f, BBA, BBC},
        {0, 0x815daa, BBA, BBC},
        {0, 0x8367dd, BBA, BBC},
        {0, 0x838852, BBA, BBC},
        {0, 0x845fb3, BBA, BBC},
        {0, 0x84c722, emptyA, emptyC},
        {0, 0x86256e, BBA, BBC},
        {0, 0x864339, BBA, BBC},
        {0, 0x869be0, BGA, BBC},
        {0, 0x895222, BBA, BBC},
        {0, 0x8c1a56, emptyA, emptyC},
        {0, 0x8cb1da, BBA, BBC},
        {0, 0x8e954e, BBA, BGC},
        {0, 0x8fe645, BBA, BBC},
        {0, 0x91bafc, BBA, BBC},
        {0, 0x93ae80, BBA, BBC},
        {0, 0x94f793, BBA, BBC},
        {0, 0x94fbfb, BBA, BBC},
        {0, 0x954c20, emptyA, emptyC},
        {0, 0x965e0f, BBA, BBC},
        {0, 0x96a30e, BBA, BBC},
        {0, 0x972f1d, BBA, BBC},
        {0, 0x98fd10, BBA, BBC},
        {0, 0x9a0472, BBA, BBC},
        {0, 0x9c4894, BBA, BBC},
        {0, 0x9e6dc8, emptyA, emptyC},
        {0, 0x9f5e8b, BBA, BBC},
        {0, 0xa035bb, BBA, BGC},
        {0, 0xa53f7c, BBA, BBC},
        {0, 0xa56537, BBA, BBC},
        {0, 0xa60957, BBA, BBC},
        {0, 0xa689db, BBA, BBC},
        {0, 0xa6d494, BBA, BBC},
        {0, 0xa6d935, BBA, BBC},
        {0, 0xa8671a, BBA, BBC},
        {0, 0xaa7956, BBA, BBC},
        {0, 0xaee392, BBA, BBC},
        {0, 0xb1d576, BBA, BBC},
        {0, 0xb235c3, BBA, BGC},
        {0, 0xb24b2d, BBA, BBC},
        {0, 0xb2f999, emptyA, emptyC},
        {0, 0xb5acac, BBA, BBC},
        {0, 0xb60ef4, BBA, BBC},
        {0, 0xb7f38a, BBA, BBC},
        {0, 0xb87d2b, BBA, BBC},
        {0, 0xb8d564, BGA, BBC},
        {0, 0xb9722e, BBA, BBC},
        {0, 0xb98a5b, BBA, BBC},
        {0, 0xb99f55, BBA, BBC},
        {0, 0xbda770, BBA, BBC},
        {0, 0xbe8f99, BBA, BBC},
        {0, 0xc226a0, BBA, BBC},
        {0, 0xc2b248, BBA, BBC},
        {0, 0xc56f42, emptyA, emptyC},
        {0, 0xc6ec01, emptyA, emptyC},
        {0, 0xc768f2, BBA, BBC},
        {0, 0xc7f149, BBA, BBC},
        {0, 0xc94a96, BBA, BBC},
        {0, 0xcbf834, BBA, BBC},
        {0, 0xce4b94, BBA, BBC},
        {0, 0xcf1b7b, BBA, BBC},
        {0, 0xcf7257, BBA, BBC},
        {0, 0xcffe10, BBA, BBC},
        {0, 0xd471f3, BBA, BBC},
        {0, 0xd5b0c6, BBA, BGC},
        {0, 0xd5c7e7, BBA, BBC},
        {0, 0xd8b421, emptyA, emptyC},
        {0, 0xd978ed, BBA, BBC},
        {0, 0xdbee52, BBA, emptyC},
        {0, 0xdc0425, BBA, BBC},
        {0, 0xdcc9e1, BBA, BBC},
        {0, 0xddb0e3, BBA, BBC},
        {0, 0xddd1a7, BGA, BBC},
        {0, 0xdfd2c6, BBA, BBC},
        {0, 0xe1db66, BBA, BBC},
        {0, 0xe27106, BBA, BBC},
        {0, 0xe69c79, BBA, BBC},
        {0, 0xe71d7b, BBA, BBC},
        {0, 0xe80a68, BBA, BBC},
        {0, 0xea06fc, BBA, BBC},
        {0, 0xf01c8e, BBA, BBC},
        {0, 0xf08660, BBA, BBC},
        {0, 0xf1a165, BBA, BBC},
        {0, 0xf30af3, BBA, BBC},
        {0, 0xf7635b, BGA, BBC},
        {0, 0xf7a36c, emptyA, emptyC},
        {0, 0xf9b31c, BBA, BBC},
        {0, 0xfb35f9, emptyA, emptyC},
        {0, 0xfc22dd, BBA, BBC},
        {0, 0xfe58ba, BBA, BBC},
        {0, 0xff5c1f, BBA, BBC},
        {0, 0xff93b5, emptyA, emptyC},
        {1, 0x3513, BBA, BBC},
        {1, 0x20656, BBA, BBC},
        {1, 0x4345d, BBA, BBC},
        {1, 0x5ec2f, BBA, BBC},
        {1, 0x8140f, BBA, BBC},
        {1, 0x93898, BBA, BBC},
        {1, 0xef46f, BBA, BBC},
        {1, 0xfff01, BGA, BBC},
        {1, 0x10ce3e, BBA, BBC},
        {1, 0x117f6a, BBA, BBC},
        {1, 0x12cdd1, BBA, emptyC},
        {1, 0x14b973, BBA, BGC},
        {1, 0x155e84, BBA, BBC},
        {1, 0x181dda, BBA, BBC},
        {1, 0x1bd5b5, BBA, BBC},
        {1, 0x1ca304, BBA, BBC},
        {1, 0x1d095a, emptyA, emptyC},
        {1, 0x1e0125, BBA, BBC},
        {1, 0x24a1c5, BBA, BBC},
        {1, 0x24c960, BBA, BBC},
        {1, 0x2638f4, BBA, BBC},
        {1, 0x27cdee, BBA, BBC},
        {1, 0x283e0d, BBA, BBC},
        {1, 0x29b1f3, BBA, BBC},
        {1, 0x2b43d4, BBA, BBC},
        {1, 0x2bb918, BBA, BBC},
        {1, 0x2d7591, BBA, BBC},
        {1, 0x2da5ac, BGA, BBC},
        {1, 0x2e2d65, BBA, BBC},
        {1, 0x33bef9, BBA, BBC},
        {1, 0x35505d, BBA, BBC},
        {1, 0x36084d, BBA, BBC},
        {1, 0x380b1f, emptyA, emptyC},
        {1, 0x38d478, BBA, BBC},
        {1, 0x3a0622, BBA, BBC},
        {1, 0x3a194e, emptyA, emptyC},
        {1, 0x3b5972, emptyA, emptyC},
        {1, 0x3ed6f5, BBA, BBC},
        {1, 0x3ef093, BBA, BBC},
        {1, 0x422847, BBA, BBC},
        {1, 0x426bbb, BBA, BBC},
        {1, 0x4b2f9b, BBA, BBC},
        {1, 0x4c5781, emptyA, emptyC},
        {1, 0x4f1137, BBA, BBC},
        {1, 0x4fe5ae, BBA, BBC},
        {1, 0x500dbc, BBA, BBC},
        {1, 0x502a5e, BGA, BBC},
        {1, 0x505769, BBA, BBC},
        {1, 0x507bd3, BBA, BBC},
        {1, 0x55b3ef, BBA, BBC},
        {1, 0x56333f, BBA, BBC},
        {1, 0x587134, BBA, BBC},
        {1, 0x5b4847, BBA, BGC},
        {1, 0x5b777a, BBA, BBC},
        {1, 0x5b7dde, BBA, BBC},
        {1, 0x5b7e14, BBA, BBC},
        {1, 0x5cd9ca, BBA, BBC},
        {1, 0x5e7c42, BBA, BBC},
        {1, 0x5facec, BBA, BBC},
        {1, 0x6030ae, BBA, BBC},
        {1, 0x64a772, BBA, BBC},
        {1, 0x687dbe, BBA, BBC},
        {1, 0x68c5dd, BBA, BGC},
        {1, 0x692064, BBA, BBC},
        {1, 0x6949da, BBA, BBC},
        {1, 0x6db110, BBA, BBC},
        {1, 0x6eab78, BBA, BBC},
        {1, 0x6fc13d, BGA, BBC},
        {1, 0x7463b6, BBA, BBC},
        {1, 0x749cec, BBA, BBC},
        {1, 0x756547, BBA, BBC},
        {1, 0x77819e, BBA, BBC},
        {1, 0x785e0b, BBA, BBC},
        {1, 0x7b3caa, BBA, BBC},
        {1, 0x7cccbb, BBA, BBC},
        {1, 0x7e7e17, BBA, BBC},
        {1, 0x7fc0f5, BBA, BBC},
        {1, 0x806ee5, BBA, emptyC} */
        };
      
      Bool_t found = kFALSE;
      
      for (Int_t i=1; i<kMaxVZero; i++)
      {
        if (fESD->GetPeriodNumber() == vzeroAnalysis[i-1][0] && fESD->GetOrbitNumber() == vzeroAnalysis[i-1][1])
        {
          found = kTRUE;
          Int_t vZero = -1;
          if (vzeroAnalysis[i][2] == emptyA && vzeroAnalysis[i][3] == BBC)
            vZero = 1;
          if (vzeroAnalysis[i][2] == BBA && vzeroAnalysis[i][3] == emptyC)
            vZero = 2;
          if (vzeroAnalysis[i][2] == emptyA && vzeroAnalysis[i][3] == emptyC)
            vZero = 3;
          if (vzeroAnalysis[i][2] == BBA && vzeroAnalysis[i][3] == BBC)
            vZero = 4;
          if (vzeroAnalysis[i][2] == BGA && vzeroAnalysis[i][3] == BBC)
            vZero = 5;
          if (vzeroAnalysis[i][2] == BBA && vzeroAnalysis[i][3] == BGC)
            vZero = 6;
            
          fStats2->Fill(decision, vZero);
          break;
        }
      }
      
      if (!found)
        fStats2->Fill(decision, 0);
      
      if (decision == 3 || decision == 4)
      {
        Printf("Skipping event %d: Period number: %d Orbit number: %x", decision, fESD->GetPeriodNumber(), fESD->GetOrbitNumber());
        return;
      }
    }
      
    fStats->Fill(5);
    
    // trigger definition
    static AliTriggerAnalysis* triggerAnalysis = new AliTriggerAnalysis;
    eventTriggered = triggerAnalysis->IsTriggerFired(fESD, fTrigger);
    if (eventTriggered)
      fStats->Fill(3);

    // get the ESD vertex
    vtxESD = AliPWG0Helper::GetVertex(fESD, fAnalysisMode);
    
    //Printf("vertex is: %s", fESD->GetPrimaryVertex()->GetTitle());

    Double_t vtx[3];

    // fill z vertex resolution
    if (vtxESD)
    {
      fVertexResolution->Fill(vtxESD->GetZRes());
      //if (strcmp(vtxESD->GetTitle(), "vertexer: 3D") == 0)
      {
        fVertex->Fill(vtxESD->GetXv(), vtxESD->GetYv(), vtxESD->GetZv());
      }
      
      if (AliPWG0Helper::TestVertex(vtxESD, fAnalysisMode))
      {
          vtxESD->GetXYZ(vtx);

          // vertex stats
          if (strcmp(vtxESD->GetTitle(), "vertexer: 3D") == 0)
          {
            fStats->Fill(1);
            if (fCheckEventType && TMath::Abs(vtx[0] > 0.3))
            {
              Printf("Suspicious x-vertex x=%f y=%f z=%f (period: %d orbit %x)", vtx[0], vtx[1], vtx[2], fESD->GetPeriodNumber(), fESD->GetOrbitNumber());
            }
            if (fCheckEventType && vtx[1] < 0.05 || vtx[1] > 0.5)
            {
              Printf("Suspicious y-vertex x=%f y=%f z=%f (period: %d orbit %x)", vtx[0], vtx[1], vtx[2], fESD->GetPeriodNumber(), fESD->GetOrbitNumber());
            }
          }
          else if (strcmp(vtxESD->GetTitle(), "vertexer: Z") == 0)
          {
            fStats->Fill(2);
          }
      }
      else
        vtxESD = 0;
    }

    // needed for syst. studies
    AliStack* stack = 0;
    TArrayF vtxMC(3);

    if (fUseMCVertex || fUseMCKine || fOnlyPrimaries || fReadMC) {
      if (!fReadMC) {
        Printf("ERROR: fUseMCVertex or fUseMCKine or fOnlyPrimaries set without fReadMC set!");
        return;
      }

      AliMCEventHandler* eventHandler = dynamic_cast<AliMCEventHandler*> (AliAnalysisManager::GetAnalysisManager()->GetMCtruthEventHandler());
      if (!eventHandler) {
        Printf("ERROR: Could not retrieve MC event handler");
        return;
      }

      AliMCEvent* mcEvent = eventHandler->MCEvent();
      if (!mcEvent) {
        Printf("ERROR: Could not retrieve MC event");
        return;
      }

      AliHeader* header = mcEvent->Header();
      if (!header)
      {
        AliDebug(AliLog::kError, "Header not available");
        return;
      }

      // get the MC vertex
      AliGenEventHeader* genHeader = header->GenEventHeader();
      if (!genHeader)
      {
        AliDebug(AliLog::kError, "Could not retrieve genHeader from Header");
        return;
      }
      genHeader->PrimaryVertex(vtxMC);

      if (fUseMCVertex)
        vtx[2] = vtxMC[2];

      stack = mcEvent->Stack();
      if (!stack)
      {
        AliDebug(AliLog::kError, "Stack not available");
        return;
      }
    }

    // create list of (label, eta, pt) tuples
    Int_t inputCount = 0;
    Int_t* labelArr = 0;
    Float_t* etaArr = 0;
    Float_t* thirdDimArr = 0;
    if (fAnalysisMode & AliPWG0Helper::kSPD)
    {
      // get tracklets
      const AliMultiplicity* mult = fESD->GetMultiplicity();
      if (!mult)
      {
        AliDebug(AliLog::kError, "AliMultiplicity not available");
        return;
      }

      labelArr = new Int_t[mult->GetNumberOfTracklets()];
      etaArr = new Float_t[mult->GetNumberOfTracklets()];
      thirdDimArr = new Float_t[mult->GetNumberOfTracklets()];

      // get multiplicity from SPD tracklets
      for (Int_t i=0; i<mult->GetNumberOfTracklets(); ++i)
      {
        //printf("%d %f %f %f\n", i, mult->GetTheta(i), mult->GetPhi(i), mult->GetDeltaPhi(i));

        if (fOnlyPrimaries)
          if (mult->GetLabel(i, 0) < 0 || mult->GetLabel(i, 0) != mult->GetLabel(i, 1) || !stack->IsPhysicalPrimary(mult->GetLabel(i, 0)))
            continue;

        Float_t deltaPhi = mult->GetDeltaPhi(i);
        // prevent values to be shifted by 2 Pi()
        if (deltaPhi < -TMath::Pi())
          deltaPhi += TMath::Pi() * 2;
        if (deltaPhi > TMath::Pi())
          deltaPhi -= TMath::Pi() * 2;

        if (TMath::Abs(deltaPhi) > 1)
          printf("WARNING: Very high Delta Phi: %d %f %f %f\n", i, mult->GetTheta(i), mult->GetPhi(i), deltaPhi);

        Int_t label = mult->GetLabel(i, 0);
        Float_t eta = mult->GetEta(i);
        
        // control histograms
        Float_t phi = mult->GetPhi(i);
        if (phi < 0)
          phi += TMath::Pi() * 2;
        fPhi->Fill(phi);
        fEtaPhi->Fill(eta, phi);
        
        if (deltaPhi < 0.01)
        {
          // layer 0
          Float_t z = vtx[2] + 3.9 / TMath::Tan(2 * TMath::ATan(TMath::Exp(- eta)));
          fZPhi[0]->Fill(z, phi);
          // layer 1
          z = vtx[2] + 7.6 / TMath::Tan(2 * TMath::ATan(TMath::Exp(- eta)));
          fZPhi[1]->Fill(z, phi);
        }

        if (vtxESD && TMath::Abs(vtx[2]) < 10)
        {
          fDeltaPhi->Fill(deltaPhi);
          fDeltaTheta->Fill(mult->GetDeltaTheta(i));
        }
        
        if (fDeltaPhiCut > 0 && TMath::Abs(deltaPhi) > fDeltaPhiCut)
          continue;

        if (fUseMCKine)
        {
          if (label > 0)
          {
            TParticle* particle = stack->Particle(label);
            eta = particle->Eta();
            phi = particle->Phi();
          }
          else
            Printf("WARNING: fUseMCKine set without fOnlyPrimaries and no label found");
        }
        
        if (fSymmetrize)
          eta = TMath::Abs(eta);

        etaArr[inputCount] = eta;
        labelArr[inputCount] = label;
        thirdDimArr[inputCount] = phi;
        ++inputCount;
      }

      if (!fFillPhi)
      {
        // fill multiplicity in third axis
        for (Int_t i=0; i<inputCount; ++i)
          thirdDimArr[i] = inputCount;
      }

      Int_t firedChips = mult->GetNumberOfFiredChips(0) + mult->GetNumberOfFiredChips(1);
      fFiredChips->Fill(firedChips, inputCount);
      Printf("Accepted %d tracklets (%d fired chips)", inputCount, firedChips);
      
      fTrackletsVsUnassigned->Fill(inputCount, mult->GetNumberOfSingleClusters());
    }
    else if (fAnalysisMode & AliPWG0Helper::kTPC || fAnalysisMode & AliPWG0Helper::kTPCITS)
    {
      if (!fEsdTrackCuts)
      {
        AliDebug(AliLog::kError, "fESDTrackCuts not available");
        return;
      }

      if (vtxESD)
      {
        // get multiplicity from ESD tracks
        TObjArray* list = fEsdTrackCuts->GetAcceptedTracks(fESD, fAnalysisMode & AliPWG0Helper::kTPC);
        Int_t nGoodTracks = list->GetEntries();
        Printf("Accepted %d tracks out of %d total ESD tracks", nGoodTracks, fESD->GetNumberOfTracks());
  
        labelArr = new Int_t[nGoodTracks];
        etaArr = new Float_t[nGoodTracks];
        thirdDimArr = new Float_t[nGoodTracks];
  
        // loop over esd tracks
        for (Int_t i=0; i<nGoodTracks; i++)
        {
          AliESDtrack* esdTrack = dynamic_cast<AliESDtrack*> (list->At(i));
          if (!esdTrack)
          {
            AliDebug(AliLog::kError, Form("ERROR: Could not retrieve track %d.", i));
            continue;
          }
          
          Float_t phi = esdTrack->Phi();
          if (phi < 0)
            phi += TMath::Pi() * 2;
  
          Float_t eta = esdTrack->Eta();
          Int_t label = TMath::Abs(esdTrack->GetLabel());
          Float_t pT  = esdTrack->Pt();
          
          // force pT to fixed value without B field
          if (!(fAnalysisMode & AliPWG0Helper::kFieldOn))
            pT = 1;
  
          fPhi->Fill(phi);
          fEtaPhi->Fill(eta, phi);
          if (eventTriggered && vtxESD)
            fRawPt->Fill(pT);
  
          if (fOnlyPrimaries)
          {
            if (label == 0)
              continue;
            
            if (stack->IsPhysicalPrimary(label) == kFALSE)
              continue;
          }
  
          if (fUseMCKine)
          {
            if (label > 0)
            {
              TParticle* particle = stack->Particle(label);
              eta = particle->Eta();
              pT = particle->Pt();
            }
            else
              Printf("WARNING: fUseMCKine set without fOnlyPrimaries and no label found");
          }
  
          if (fSymmetrize)
            eta = TMath::Abs(eta);
          etaArr[inputCount] = eta;
          labelArr[inputCount] = TMath::Abs(esdTrack->GetLabel());
          thirdDimArr[inputCount] = pT;
          ++inputCount;
        }
        
        // TODO restrict inputCount used as measure for the multiplicity to |eta| < 1
  
        delete list;
      }
    }
    else
      return;

    // Processing of ESD information (always)
    if (eventTriggered)
    {
      // control hist
      fMult->Fill(inputCount);
      fdNdEtaAnalysisESD->FillTriggeredEvent(inputCount);

      fTriggerVsTime->SetPoint(fTriggerVsTime->GetN(), fESD->GetTimeStamp(), 1);

      if (vtxESD)
      {
        // control hist
        
        if (strcmp(vtxESD->GetTitle(), "vertexer: 3D") == 0)
          fVertexVsMult->Fill(vtxESD->GetXv(), vtxESD->GetYv(), inputCount);
      
        fMultVtx->Fill(inputCount);

        for (Int_t i=0; i<inputCount; ++i)
        {
          Float_t eta = etaArr[i];
          Float_t thirdDim = thirdDimArr[i];

          fdNdEtaAnalysisESD->FillTrack(vtx[2], eta, thirdDim);

          if (TMath::Abs(vtx[2]) < 10)
          {
            fPartEta[0]->Fill(eta);

            if (vtx[2] < 0)
              fPartEta[1]->Fill(eta);
            else
              fPartEta[2]->Fill(eta);
          }
        }

        // for event count per vertex
        fdNdEtaAnalysisESD->FillEvent(vtx[2], inputCount);

        // control hist
	if (inputCount > 0)
	        fEvents->Fill(vtx[2]);

        if (fReadMC)
        {
          // from tracks is only done for triggered and vertex reconstructed events
          for (Int_t i=0; i<inputCount; ++i)
          {
            Int_t label = labelArr[i];

            if (label < 0)
              continue;

            //Printf("Getting particle of track %d", label);
            TParticle* particle = stack->Particle(label);

            if (!particle)
            {
              AliDebug(AliLog::kError, Form("ERROR: Could not retrieve particle %d.", label));
              continue;
            }

            Float_t thirdDim = -1;
            if (fAnalysisMode & AliPWG0Helper::kSPD)
            {
              if (fFillPhi)
              {
                thirdDim = particle->Phi();
              }
              else
                thirdDim = inputCount;
            }
            else
              thirdDim = particle->Pt();

            Float_t eta = particle->Eta();
            if (fSymmetrize)
              eta = TMath::Abs(eta);
            fdNdEtaAnalysisTracks->FillTrack(vtxMC[2], eta, thirdDim);
          } // end of track loop

          // for event count per vertex
          fdNdEtaAnalysisTracks->FillEvent(vtxMC[2], inputCount);
        }
      }
    }

    if (etaArr)
      delete[] etaArr;
    if (labelArr)
      delete[] labelArr;
    if (thirdDimArr)
      delete[] thirdDimArr;
  }

  if (fReadMC)   // Processing of MC information (optional)
  {
    AliMCEventHandler* eventHandler = dynamic_cast<AliMCEventHandler*> (AliAnalysisManager::GetAnalysisManager()->GetMCtruthEventHandler());
    if (!eventHandler) {
      Printf("ERROR: Could not retrieve MC event handler");
      return;
    }

    AliMCEvent* mcEvent = eventHandler->MCEvent();
    if (!mcEvent) {
      Printf("ERROR: Could not retrieve MC event");
      return;
    }

    AliStack* stack = mcEvent->Stack();
    if (!stack)
    {
      AliDebug(AliLog::kError, "Stack not available");
      return;
    }

    AliHeader* header = mcEvent->Header();
    if (!header)
    {
      AliDebug(AliLog::kError, "Header not available");
      return;
    }

    // get the MC vertex
    AliGenEventHeader* genHeader = header->GenEventHeader();
    if (!genHeader)
    {
      AliDebug(AliLog::kError, "Could not retrieve genHeader from Header");
      return;
    }

    TArrayF vtxMC(3);
    genHeader->PrimaryVertex(vtxMC);

    // get process type
    Int_t processType = AliPWG0Helper::GetEventProcessType(header);
    AliDebug(AliLog::kDebug+1, Form("Found process type %d", processType));

    if (processType==AliPWG0Helper::kInvalidProcess)
      AliDebug(AliLog::kError, Form("Unknown process type %d.", processType));

    // loop over mc particles
    Int_t nPrim  = stack->GetNprimary();

    Int_t nAcceptedParticles = 0;

    // count particles first, then fill
    for (Int_t iMc = 0; iMc < nPrim; ++iMc)
    {
      if (!stack->IsPhysicalPrimary(iMc))
        continue;

      //Printf("Getting particle %d", iMc);
      TParticle* particle = stack->Particle(iMc);

      if (!particle)
        continue;

      if (particle->GetPDG()->Charge() == 0)
        continue;

      //AliDebug(AliLog::kDebug+1, Form("Accepted primary %d, unique ID: %d", iMc, particle->GetUniqueID()));
      Float_t eta = particle->Eta();

       // make a rough eta cut (so that nAcceptedParticles is not too far off the true measured value (NB: this histograms are only gathered for comparison))
      if (TMath::Abs(eta) < 1.5) // && pt > 0.3)
        nAcceptedParticles++;
    }

    for (Int_t iMc = 0; iMc < nPrim; ++iMc)
    {
      if (!stack->IsPhysicalPrimary(iMc))
        continue;

      //Printf("Getting particle %d", iMc);
      TParticle* particle = stack->Particle(iMc);

      if (!particle)
        continue;

      if (particle->GetPDG()->Charge() == 0)
        continue;

      Float_t eta = particle->Eta();
      if (fSymmetrize)
        eta = TMath::Abs(eta);

      Float_t thirdDim = -1;

      if (fAnalysisMode & AliPWG0Helper::kSPD)
      {
        if (fFillPhi)
        {
          thirdDim = particle->Phi();
        }
        else
          thirdDim = nAcceptedParticles;
      }
      else
        thirdDim = particle->Pt();

      fdNdEtaAnalysis->FillTrack(vtxMC[2], eta, thirdDim);

      if (processType != AliPWG0Helper::kSD)
        fdNdEtaAnalysisNSD->FillTrack(vtxMC[2], eta, thirdDim);

      if (processType == AliPWG0Helper::kND)
        fdNdEtaAnalysisND->FillTrack(vtxMC[2], eta, thirdDim);

      if (eventTriggered)
      {
        fdNdEtaAnalysisTr->FillTrack(vtxMC[2], eta, thirdDim);
        if (vtxESD)
          fdNdEtaAnalysisTrVtx->FillTrack(vtxMC[2], eta, thirdDim);
      }

      if (TMath::Abs(eta) < 1.0 && particle->Pt() > 0 && particle->P() > 0)
      {
        //Float_t value = 1. / TMath::TwoPi() / particle->Pt() * particle->Energy() / particle->P();
        Float_t value = 1;
        fPartPt->Fill(particle->Pt(), value);
      }
    }

    fdNdEtaAnalysis->FillEvent(vtxMC[2], nAcceptedParticles);
    if (processType != AliPWG0Helper::kSD)
      fdNdEtaAnalysisNSD->FillEvent(vtxMC[2], nAcceptedParticles);
    if (processType == AliPWG0Helper::kND)
      fdNdEtaAnalysisND->FillEvent(vtxMC[2], nAcceptedParticles);

    if (eventTriggered)
    {
      fdNdEtaAnalysisTr->FillEvent(vtxMC[2], nAcceptedParticles);
      if (vtxESD)
        fdNdEtaAnalysisTrVtx->FillEvent(vtxMC[2], nAcceptedParticles);
    }
  }
}

void AlidNdEtaTask::Terminate(Option_t *)
{
  // The Terminate() function is the last function to be called during
  // a query. It always runs on the client, it can be used to present
  // the results graphically or save the results to file.

  fOutput = dynamic_cast<TList*> (GetOutputData(0));
  if (!fOutput)
    Printf("ERROR: fOutput not available");

  if (fOutput)
  {
    fdNdEtaAnalysisESD = dynamic_cast<dNdEtaAnalysis*> (fOutput->FindObject("fdNdEtaAnalysisESD"));
    fMult = dynamic_cast<TH1F*> (fOutput->FindObject("fMult"));
    fMultVtx = dynamic_cast<TH1F*> (fOutput->FindObject("fMultVtx"));
    for (Int_t i=0; i<3; ++i)
      fPartEta[i] = dynamic_cast<TH1F*> (fOutput->FindObject(Form("dndeta_check_%d", i)));
    fEvents = dynamic_cast<TH1F*> (fOutput->FindObject("dndeta_check_vertex"));
    fVertexResolution = dynamic_cast<TH1F*> (fOutput->FindObject("dndeta_vertex_resolution_z"));

    fVertex = dynamic_cast<TH3F*> (fOutput->FindObject("vertex_check"));
    fVertexVsMult = dynamic_cast<TH3F*> (fOutput->FindObject("fVertexVsMult"));
    fPhi = dynamic_cast<TH1F*> (fOutput->FindObject("fPhi"));
    fRawPt = dynamic_cast<TH1F*> (fOutput->FindObject("fRawPt"));
    fEtaPhi = dynamic_cast<TH2F*> (fOutput->FindObject("fEtaPhi"));
    for (Int_t i=0; i<2; ++i)
      fZPhi[i] = dynamic_cast<TH2F*> (fOutput->FindObject(Form("fZPhi_%d", i)));
    fDeltaPhi = dynamic_cast<TH1F*> (fOutput->FindObject("fDeltaPhi"));
    fDeltaTheta = dynamic_cast<TH1F*> (fOutput->FindObject("fDeltaTheta"));
    fFiredChips = dynamic_cast<TH2F*> (fOutput->FindObject("fFiredChips"));
    fTrackletsVsClusters = dynamic_cast<TH2F*> (fOutput->FindObject("fTrackletsVsClusters"));
    fTrackletsVsUnassigned = dynamic_cast<TH2F*> (fOutput->FindObject("fTrackletsVsUnassigned"));
    fTriggerVsTime = dynamic_cast<TGraph*> (fOutput->FindObject("fTriggerVsTime"));
    fStats = dynamic_cast<TH1F*> (fOutput->FindObject("fStats"));
    fStats2 = dynamic_cast<TH2F*> (fOutput->FindObject("fStats2"));

    fEsdTrackCuts = dynamic_cast<AliESDtrackCuts*> (fOutput->FindObject("fEsdTrackCuts"));
  }

  if (!fdNdEtaAnalysisESD)
  {
    AliDebug(AliLog::kError, "ERROR: fdNdEtaAnalysisESD not available");
  }
  else
  {
    if (fMult && fMultVtx)
    {
      new TCanvas;
      fMult->Draw();
      fMultVtx->SetLineColor(2);
      fMultVtx->Draw("SAME");
    }

    if (fFiredChips)
    {
      new TCanvas;
      fFiredChips->Draw("COLZ");
    }

    if (fPartEta[0])
    {
      Int_t events1 = (Int_t) fEvents->Integral(fEvents->GetXaxis()->FindBin(-19.9), fEvents->GetXaxis()->FindBin(-0.001));
      Int_t events2 = (Int_t) fEvents->Integral(fEvents->GetXaxis()->FindBin(0.001), fEvents->GetXaxis()->FindBin(19.9));

      Printf("%d events with vertex used", events1 + events2);

      if (events1 > 0 && events2 > 0)
      {
        fPartEta[0]->Scale(1.0 / (events1 + events2));
        fPartEta[1]->Scale(1.0 / events1);
        fPartEta[2]->Scale(1.0 / events2);

        for (Int_t i=0; i<3; ++i)
          fPartEta[i]->Scale(1.0 / fPartEta[i]->GetBinWidth(1));

        new TCanvas("control", "control", 500, 500);
        for (Int_t i=0; i<3; ++i)
        {
          fPartEta[i]->SetLineColor(i+1);
          fPartEta[i]->Draw((i==0) ? "" : "SAME");
        }
      }
    }

    if (fEvents)
    {
        new TCanvas("control3", "control3", 500, 500);
        fEvents->Draw();
    }

    TFile* fout = new TFile("analysis_esd_raw.root", "RECREATE");

    if (fdNdEtaAnalysisESD)
      fdNdEtaAnalysisESD->SaveHistograms();

    if (fEsdTrackCuts)
      fEsdTrackCuts->SaveHistograms("esd_track_cuts");

    if (fMult)
      fMult->Write();

    if (fMultVtx)
      fMultVtx->Write();

    for (Int_t i=0; i<3; ++i)
      if (fPartEta[i])
        fPartEta[i]->Write();

    if (fEvents)
      fEvents->Write();

    if (fVertexResolution)
      fVertexResolution->Write();

    if (fDeltaPhi)
      fDeltaPhi->Write();

    if (fDeltaTheta)
      fDeltaTheta->Write();
    
    if (fPhi)
      fPhi->Write();

    if (fRawPt)
      fRawPt->Write();

    if (fEtaPhi)
      fEtaPhi->Write();

    for (Int_t i=0; i<2; ++i)
      if (fZPhi[i])
        fZPhi[i]->Write();
    
    if (fFiredChips)
      fFiredChips->Write();

    if (fTrackletsVsClusters)
      fTrackletsVsClusters->Write();
    
    if (fTrackletsVsUnassigned)
      fTrackletsVsUnassigned->Write();
    
    if (fTriggerVsTime)
      fTriggerVsTime->Write();

    if (fStats)
      fStats->Write();

    if (fStats2)
      fStats2->Write();
    
    if (fVertex)
      fVertex->Write();

    if (fVertexVsMult)
      fVertexVsMult->Write();
    
    fout->Write();
    fout->Close();

    Printf("Writting result to analysis_esd_raw.root");
  }

  if (fReadMC)
  {
    if (fOutput)
    {
      fdNdEtaAnalysis = dynamic_cast<dNdEtaAnalysis*> (fOutput->FindObject("dndeta"));
      fdNdEtaAnalysisND = dynamic_cast<dNdEtaAnalysis*> (fOutput->FindObject("dndetaND"));
      fdNdEtaAnalysisNSD = dynamic_cast<dNdEtaAnalysis*> (fOutput->FindObject("dndetaNSD"));
      fdNdEtaAnalysisTr = dynamic_cast<dNdEtaAnalysis*> (fOutput->FindObject("dndetaTr"));
      fdNdEtaAnalysisTrVtx = dynamic_cast<dNdEtaAnalysis*> (fOutput->FindObject("dndetaTrVtx"));
      fdNdEtaAnalysisTracks = dynamic_cast<dNdEtaAnalysis*> (fOutput->FindObject("dndetaTracks"));
      fPartPt = dynamic_cast<TH1F*> (fOutput->FindObject("dndeta_check_pt"));
    }

    if (!fdNdEtaAnalysis || !fdNdEtaAnalysisTr || !fdNdEtaAnalysisTrVtx || !fPartPt)
    {
      AliDebug(AliLog::kError, Form("ERROR: Histograms not available %p %p", (void*) fdNdEtaAnalysis, (void*) fPartPt));
      return;
    }

    fdNdEtaAnalysis->Finish(0, -1, AlidNdEtaCorrection::kNone);
    fdNdEtaAnalysisND->Finish(0, -1, AlidNdEtaCorrection::kNone);
    fdNdEtaAnalysisNSD->Finish(0, -1, AlidNdEtaCorrection::kNone);
    fdNdEtaAnalysisTr->Finish(0, -1, AlidNdEtaCorrection::kNone);
    fdNdEtaAnalysisTrVtx->Finish(0, -1, AlidNdEtaCorrection::kNone);
    fdNdEtaAnalysisTracks->Finish(0, -1, AlidNdEtaCorrection::kNone);

    Int_t events = (Int_t) fdNdEtaAnalysis->GetData()->GetEventCorrection()->GetMeasuredHistogram()->Integral();
    fPartPt->Scale(1.0/events);
    fPartPt->Scale(1.0/fPartPt->GetBinWidth(1));

    TFile* fout = new TFile("analysis_mc.root","RECREATE");

    fdNdEtaAnalysis->SaveHistograms();
    fdNdEtaAnalysisND->SaveHistograms();
    fdNdEtaAnalysisNSD->SaveHistograms();
    fdNdEtaAnalysisTr->SaveHistograms();
    fdNdEtaAnalysisTrVtx->SaveHistograms();
    fdNdEtaAnalysisTracks->SaveHistograms();

    if (fPartPt)
      fPartPt->Write();

    fout->Write();
    fout->Close();

    Printf("Writting result to analysis_mc.root");

    if (fPartPt)
    {
      new TCanvas("control2", "control2", 500, 500);
      fPartPt->Draw();
    }
  }
}
