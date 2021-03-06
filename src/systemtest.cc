#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/argvparser.h"
#include "../Utils/Timer.h"

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

    cmd.defineOption ( "configure", "Configure HW", ArgvParser::NoOptionAttribute );
    cmd.defineOptionAlternative ( "configure", "c" );

    int result = cmd.parse ( argc, argv );

    if ( result != ArgvParser::NoParserError )
    {
        LOG (INFO) << cmd.parseErrorDescription ( result );
        exit ( 1 );
    }

    // now query the parsing results
    std::string cHWFile = ( cmd.foundOption ( "file" ) ) ? cmd.optionValue ( "file" ) : "settings/HWDescription_2CBC.xml";
    bool cConfigure = ( cmd.foundOption ( "configure" ) ) ? true : false;


    std::stringstream outp;
    SystemController cSystemController;
    cSystemController.InitializeHw ( cHWFile, outp );
    cSystemController.InitializeSettings ( cHWFile, outp );

    LOG (INFO) << outp.str();
    outp.str ("");

    if ( cConfigure ) cSystemController.ConfigureHw (outp);

    LOG (INFO) << outp.str();

    //Timer t;
    //t.start();

    //for(uint8_t cVcth = 0x00; cVcth < 0xFF; cVcth++)
    //{
    //std::vector<std::pair<std::string, uint8_t>> cRegVec;
    //cRegVec.push_back({"VCth", cVcth});
    //cRegVec.push_back({"TriggerLatency", 255 - cVcth});
    //std::cout << "Writing Vcth " << +cVcth << " Trigger Lat:" << 255-cVcth << std::endl;
    //cSystemController.fCbcInterface->WriteBroadcastMultReg(cSystemController.fBoardVector.at(0)->fModuleVector.at(0), cRegVec);
    //CbcRegReader cReader(cSystemController.fCbcInterface, "TriggerLatency");
    //cSystemController.accept(cReader);
    //}
    //t.stop();
    //t.show("Time to loop VCth from 0 to ff with broadcast:");

    LOG (INFO) << "*** End of the System test ***" ;
    cSystemController.Destroy();
    return 0;
}
