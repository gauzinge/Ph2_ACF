#include "SystemController.h"
#include "CommonVisitors.h"
#include "argvparser.h"
#include "Timer.h"
#include "UsbUtilities.h"
#include "BiasSweep.h"
#include "StubSweep.h"
#include "TROOT.h"
#include "TApplication.h"
#include <sys/wait.h>
#ifdef __USBINST__
#include <zmq.hpp>
#include "AppLock.cc"
#include "HMP4040Controller.h"
#include "HMP4040Client.h"
#include "Ke2110Controller.h"
using namespace Ph2_UsbInst;
#endif
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP
int main ( int argc, char** argv )
{
    //configure the logger
    el::Configurations conf ("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers (conf);
    ArgvParser cmd;
    // init
    cmd.setIntroductoryDescription ( "CMS Ph2_ACF  system test application" );
    // error codes
    cmd.addErrorCode ( 0, "Success" );
    cmd.addErrorCode ( 1, "Error" );
    // options
    cmd.setHelpOption ( "h", "help", "Print this help page" );

    cmd.defineOption ( "file", "Hw Description File . Default value: settings/HWDescription_2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
    cmd.defineOptionAlternative ( "file", "f" );

    cmd.defineOption ( "output", "Output Directory . Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
    cmd.defineOptionAlternative ( "output", "o" );

    cmd.defineOption ( "sweep", "test the bias sweep tool", ArgvParser::NoOptionAttribute );
    cmd.defineOptionAlternative ( "sweep", "s" );

    cmd.defineOption ( "stubs", "test the stub sweep tool", ArgvParser::NoOptionAttribute );

    cmd.defineOption ( "events", "Number of Events . Default value: 10", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
    cmd.defineOptionAlternative ( "events", "e" );

    cmd.defineOption ( "vcth", "Threshold in VCth units (hex (including 0x) or decimal) . Default values from HW description .XML file", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/ );
    cmd.defineOptionAlternative ( "vcth", "v" );

    cmd.defineOption ( "batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute );
    cmd.defineOptionAlternative ( "batch", "b" );

    int result = cmd.parse ( argc, argv );

    if ( result != ArgvParser::NoParserError )
    {
        LOG (INFO) << cmd.parseErrorDescription ( result );
        exit ( 1 );
    }

    // now query the parsing results
    std::string cHWFile = ( cmd.foundOption ( "file" ) ) ? cmd.optionValue ( "file" ) : "settings/Cbc3HWDescription.xml";
    //bool cConfigure = ( cmd.foundOption ( "configure" ) ) ? true : false;
    uint32_t pEventsperVcth = ( cmd.foundOption ( "events" ) ) ? convertAnyInt ( cmd.optionValue ( "events" ).c_str() ) : 10;
    bool cVcthset = cmd.foundOption ("vcth");
    uint16_t cVcth = ( cmd.foundOption ( "vcth" ) ) ? convertAnyInt ( cmd.optionValue ( "vcth" ).c_str() ) : 200;
    std::string cDirectory = ( cmd.foundOption ( "output" ) ) ? cmd.optionValue ( "output" ) : "Results/";
    bool batchMode = ( cmd.foundOption ( "batch" ) ) ? true : false;
    bool cSweep = ( cmd.foundOption ( "sweep" ) ) ? true : false;
    bool cStubSweep = ( cmd.foundOption ( "stubs" ) ) ? true : false;

    std::string cResultfile;

    if (cSweep && !cStubSweep) cDirectory += "BiasSweep", cResultfile = "BiasSweep";
    else if (cStubSweep && ! cSweep) cDirectory += "StubSweep", cResultfile = "StubSweep";
    else if (cStubSweep && cSweep) cDirectory += "Cbc3Test", cResultfile = "Cbc3Test";

    TApplication cApp ( "Root Application", &argc, argv );

    if ( batchMode ) gROOT->SetBatch ( true );
    else TQObject::Connect ( "TCanvas", "Closed()", "TApplication", &cApp, "Terminate()" );

#ifdef __USBINST__
    //controller and clients should be in main
    Ke2110Controller* cKeController = nullptr;
    HMP4040Client* cLVClient = nullptr;
#endif


    //Start server to communicate with HMP404 instrument via usbtmc and SCPI
    std::string cHostname = "localhost";
    int cPowerSupplyHttpPortNumber = 8080;
    int cPowerSupplyZmqPortNumber = 8081;
    std::string cPowerSupplyHWFile = "HMP4040.xml";
    std::string cPowerSupplyOutputFile = cDirectory + "/Current_log.txt";
    int cPowerSupplyInterval = 5;
    std::pair<int, int> cPowerSupplyPortsInfo = std::pair<int, int> (cPowerSupplyZmqPortNumber, cPowerSupplyHttpPortNumber);
    pid_t cHMPPid;  // the child process that the execution will soon run inside of.
    cHMPPid = fork();
    bool cPSStatus = false;

    if (cHMPPid < 0) // fork failed
        exit (1);
    else if (cHMPPid == 0) // fork succeeded
    {
        // launch HMP4040 server
        LOG (INFO) << BOLDBLUE << "Trying to launch server to monitor currents on the HMP4040" << RESET ;
        launch_HMPServer ( cPowerSupplyHWFile, cHostname, cPowerSupplyPortsInfo, cPowerSupplyInterval );
        exit (0);
    }
    else  // Main (parent) process after fork succeeds
    {
        int returnStatus = -1 ;
        waitpid (cHMPPid, &returnStatus, 0);

        if (returnStatus == 0 ) cPSStatus = true; //child process terminated without error
        else if (returnStatus == 1)
        {
            LOG (INFO) << "The PS child process terminated with an error!." ;
            exit (1);
        }
    }

    int cDMMHttpPort = 8082;
    int cDMMZMQPort = 8083;
    std::string cDMMOutputFile = cDirectory + "/Temperature_log.txt";
    int cDMMInterval = 2;
    std::pair<int, int> cDMMPortsInfo = std::pair<int, int> (cDMMZMQPort, cDMMHttpPort);
    pid_t cDMMPid;
    cDMMPid = fork();
    bool cDMMStatus = false;

    if (cDMMPid < 0) //fork failed
        exit (1);
    else if (cDMMPid == 0 ) // fork succeeded
    {
        //launch DMM server
        LOG (INFO) << BOLDBLUE << "Trying to launch server to monitor Temperature with the Ke2110" << RESET ;
        launch_DMMServer ( cHostname, cDMMPortsInfo, cDMMInterval );
        exit (0);
    }
    else //Main (parent) process after fork succeeds
    {
        int returnStatus = -1;
        waitpid (cDMMPid, &returnStatus, 0); // Parent process waits here for child to terminate.

        if (returnStatus == 0 ) cDMMStatus = true; //child process terminated without error
        else if (returnStatus == 1)
        {
            LOG (INFO) << "The DMM child process terminated with an error!." ;
            exit (1);
        }
    }

    std::this_thread::sleep_for (std::chrono::seconds (3) );

    if ( cPSStatus && cDMMStatus )  // Verify child process terminated without error.
    {
#ifdef __USBINST__
        cLVClient = new HMP4040Client (cHostname, cPowerSupplyZmqPortNumber);
        cLVClient->StartMonitoring();
        LOG (INFO) << BOLDBLUE << "Starting monitoring of power supply currents on the HMP4040" << RESET ;
        // make sure power supply is switched  on before doing anything else
        LOG (INFO) << BOLDBLUE << "Switching on the power supply" << RESET ;
        cLVClient->ToggleOutput (1);
#endif
        std::stringstream outp;

        if (cSweep || cStubSweep)
        {
            Tool cTool;
            cTool.InitializeHw ( cHWFile, outp );
            cTool.InitializeSettings ( cHWFile, outp );
            LOG (INFO) << outp.str();
            outp.str ("");
            cTool.CreateResultDirectory ( cDirectory );
            cTool.InitResultFile (cResultfile);
            cTool.StartHttpServer (8084);
            cTool.ConfigureHw ();

            if (cSweep)
            {
                BiasSweep cSweep;
                cSweep.Inherit (&cTool);
                cSweep.Initialize();

                for (auto cBoard : cSweep.fBoardVector)
                {
                    for (auto cFe : cBoard->fModuleVector)
                    {
                        for (auto cCbc : cFe->fCbcVector)
                        {
                            cSweep.SweepBias ("Ipa", cCbc);
                            cSweep.SweepBias ("Vth", cCbc);
                        }
                    }
                }

                LOG (DEBUG) << "I get here!";
            }

            if (cStubSweep)
            {
#ifdef __USBINST__
                LOG (INFO) << BOLDBLUE << "Resetting the power to the CBC3 before attempting a stub sweep." << RESET ;
                cLVClient->ToggleOutput (0);
                cLVClient->ToggleOutput (1);
#endif

                StubSweep cSweep;
                cSweep.Inherit (&cTool);
                cSweep.Initialize();
                cSweep.SweepStubs (1);
            }

            cTool.SaveResults();
            cTool.CloseResultFile();
            cTool.Destroy();
            LOG (DEBUG) << "Get here!";
        }

        if (!cSweep && !cStubSweep)
        {
            SystemController cSystemController;
            cSystemController.InitializeHw ( cHWFile, outp );
            cSystemController.InitializeSettings ( cHWFile, outp );
            LOG (INFO) << outp.str();
            outp.str ("");
            // from here
            BeBoard* pBoard = cSystemController.fBoardVector.at ( 0 );
            uint32_t cN = 1;
            uint32_t cNthAcq = 0;
            const std::vector<Event*>* pEvents ;

            ThresholdVisitor cVisitor (cSystemController.fCbcInterface, 0);

            if (cVcthset)
            {
                cVisitor.setThreshold (cVcth);
                cSystemController.accept (cVisitor);
            }

            cSystemController.ReadNEvents (pBoard, pEventsperVcth);
            pEvents = &cSystemController.GetEvents ( pBoard );

            for ( auto& ev : *pEvents )
            {
                LOG (INFO) << ">>> Event #" << cN++ << " Threshold: " << cVcth;
                outp.str ("");
                outp << *ev;
                LOG (INFO) << outp.str();
            }

            cNthAcq++;
            cSystemController.Destroy();
        }

        LOG (DEBUG) << "But not here!";

#ifdef __USBINST__
        LOG (INFO) << YELLOW << "Toggling HMP Output off again, stopping the monitoring and exiting the server!" << RESET;
        cLVClient->ToggleOutput (0);
        cLVClient->StopMonitoring();
        cLVClient->Quit();


        cKeController = new Ke2110Controller();
        cKeController->InitializeClient ("localhost", cDMMZMQPort);
        cKeController->SendQuit();
        LOG (INFO) << YELLOW << "Stopping Temperatue monitoring and exiting the server!" << RESET;


#endif
    }
    else
        LOG (ERROR) << "Either PowerSupply or DMM server did not start up correctly!";

    LOG (INFO) << "*** End of the System test ***" ;

    if ( !batchMode )
    {
        cApp.Run();

        if (cLVClient) delete cLVClient;

        if (cKeController) delete cKeController;
    }

    else
    {
        if (cLVClient) delete cLVClient;

        if (cKeController) delete cKeController;
    }

    return 0;
}
