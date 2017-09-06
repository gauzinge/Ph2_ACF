#include <cstring>
#include "../HWDescription/Cbc.h"
#include "../HWDescription/Module.h"
#include "../HWDescription/BeBoard.h"
#include "../HWInterface/CbcInterface.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWDescription/Definition.h"
#include "../tools/CMTester.h"
#include "../tools/PedeNoise.h"
#include "../tools/Calibration.h"

#include <TApplication.h>
#include "../Utils/argvparser.h"
//#include "../Utils/easylogging++.h"
#include "TROOT.h"



using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

int main ( int argc, char* argv[] )
{
    //configure the logger
    el::Configurations conf ("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers (conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription ( "CMS Ph2_ACF  applicaton to take data and analyze it with respect to common mode noise" );
    // error codes
    cmd.addErrorCode ( 0, "Success" );
    cmd.addErrorCode ( 1, "Error" );
    // options
    cmd.setHelpOption ( "h", "help", "Print this help page" );

    cmd.defineOption ( "file", "Hw Description File . Default value: settings/CMTest2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
    cmd.defineOptionAlternative ( "file", "f" );

    cmd.defineOption ( "output", "Output Directory . Default value: Results/", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
    cmd.defineOptionAlternative ( "output", "o" );

    cmd.defineOption ( "scan", "scan for noisy strips", ArgvParser::NoOptionAttribute );
    cmd.defineOptionAlternative ( "scan", "s" );

    cmd.defineOption ( "calibrate", "Find offsets and Vcth, in case they are not set in the xml", ArgvParser::NoOptionAttribute );
    cmd.defineOptionAlternative ( "calibrate", "c" );

    cmd.defineOption ( "pedestalshift", "Shift the pedestal by N units of Vcth, default value: 0. (Only makes sense when running with --calibrate)", ArgvParser::OptionRequiresValue );
    cmd.defineOptionAlternative ( "pedestalshift", "p" );

    cmd.defineOption ( "batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute );
    cmd.defineOptionAlternative ( "batch", "b" );

    cmd.defineOption ( "gui", "option only suitable when launching from gui", ArgvParser::NoOptionAttribute );
    cmd.defineOptionAlternative ( "gui", "g" );


    int result = cmd.parse ( argc, argv );

    if ( result != ArgvParser::NoParserError )
    {
        LOG (INFO) << cmd.parseErrorDescription ( result );
        exit ( 1 );
    }

    bool isGui = ( cmd.foundOption ( "gui" ) ) ? true : false;

    // now query the parsing results
    std::string cHWFile = ( cmd.foundOption ( "file" ) ) ? cmd.optionValue ( "file" ) : "settings/CMTest2CBC.xml";
    std::string cDirectory = ( cmd.foundOption ( "output" ) ) ? cmd.optionValue ( "output" ) : "Results/";
    int cPedestalShift = ( cmd.foundOption ( "pedestalshift" ) ) ? convertAnyInt ( cmd.optionValue ( "pedestalshift" ).c_str() ) : 0;
    cDirectory += "CMTest";
    bool cScan = ( cmd.foundOption ( "scan" ) ) ? true : false;
    bool cCalibrate = ( cmd.foundOption ( "calibrate" ) ) ? true : false;
    bool batchMode = ( cmd.foundOption ( "batch" ) ) ? true : false;


    TApplication cApp ( "Root Application", &argc, argv );

    if ( batchMode ) gROOT->SetBatch ( true );
    else TQObject::Connect ( "TCanvas", "Closed()", "TApplication", &cApp, "Terminate()" );

    std::stringstream outp;


    Tool cTool;
    cTool.InitializeHw ( cHWFile, outp );
    cTool.InitializeSettings ( cHWFile, outp );
    LOG (INFO) << outp.str();
    outp.str ("");
    cTool.CreateResultDirectory ( cDirectory );
    cTool.InitResultFile ( "CMTest" );
    cTool.StartHttpServer();
    cTool.ConfigureHw ();

    if (cCalibrate) 
    {
        // Find offsets
	Calibration cCalibration;
	cCalibration.Inherit (&cTool);
	cCalibration.Initialise (false);
	cCalibration.FindVplus();
	cCalibration.FindOffsets();
	cCalibration.writeObjects();
	//cCalibration.dumpConfigFiles();
	
	// Find Vcth
	PedeNoise cPedeNoise;
	cPedeNoise.Inherit (&cTool);
	cPedeNoise.Initialise();
	cPedeNoise.measureNoise();
	cPedeNoise.Validate();
	cPedeNoise.writeObjects();

	// Set Vcth to found value
	Module* cFe = cPedeNoise.fBoardVector.at (0)->fModuleVector.at (0);
	uint16_t cPedestal = round (cPedeNoise.getPedestal (cFe) );
	ThresholdVisitor cVisitor (cTool.fCbcInterface, 0);
	cVisitor.setThreshold (cPedestal+cPedestalShift);
	cTool.accept (cVisitor);
	LOG (INFO) << "Set threshold to pedestal ("<<cPedestal<<") plus "<<cPedestalShift;
    } 

    // Careful: runs on 10*Nevents so that previous steps can move quickly
    CMTester cTester;
    cTester.Inherit (&cTool);
    //cTester.InitializeHw ( cHWFile, outp );
    //cTester.InitializeSettings ( cHWFile, outp );
    //LOG (INFO) << outp.str();
    //outp.str ("");
    //cTester.CreateResultDirectory ( cDirectory );
    //cTester.InitResultFile ( "CMTest" );
    cTester.StartHttpServer ( 8082 );
    cTester.Initialize ();

    if ( !isGui ) cTester.ConfigureHw ();
    if ( cScan ) cTester.ScanNoiseChannels();

    cTester.TakeData();
    cTester.FinishRun();
    cTester.SaveResults();
    cTester.CloseResultFile();
    cTester.Destroy();

    if ( !batchMode ) cApp.Run();

    return 0;

}
