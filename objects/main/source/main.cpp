/*!
* \file main.cpp															 
* \brief This is the Main program file which controls the initialization,
*  model looping over time steps, and outputs results  for the model.
* \author Sonny Kim
* \date $Date$
* \version $Revision$
*/

#include "util/base/include/definitions.h"

// include standard libraries
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <memory>

// xerces xml headers
#include <xercesc/dom/DOM.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include "util/base/include/xml_helper.h"

// include custom headers
#include "util/base/include/configuration.h"
#include "containers/include/world.h"
#include "util/base/include/model_time.h"
#include "containers/include/scenario.h"
#include "sectors/include/ag_sector.h"
#include "marketplace/include/marketplace.h"
#include "util/logger/include/logger_factory.h"
#include "util/logger/include/logger.h"

// Function Prototypes (for functions called by main())
extern void createDBout();
extern void openDB();
extern void createMCvarid();
extern void closeDB();

using namespace std; // enables elimination of std::
using namespace xercesc;

// define file (ofstream) objects for outputs, debugging and logs
ofstream bugoutfile,outfile,outfile2,dbout,logfile,sdcurvefile,sdfile;	

Scenario* scenario = 0; // model scenario info
time_t ltime;
int Tabs::numTabs = 0;

//******* Start of Main Program ********
int main() {

    clock_t start, afterinit, intermediate, finish;

    // Use a smart pointer for configuration so that if the main is exited before the end the memory is freed.
    auto_ptr<Configuration> confPtr( Configuration::getInstance() );
    Configuration* conf = confPtr.get();


    double duration = 0;
    ofstream xmlOutStream;

    const string configurationFileName = string( __ROOT_PREFIX__ ) + string( "configuration.xml");
    start = clock(); // start of model run
    time(&ltime); // get time and date before model run

    // Initialize the Xerces parser
    try {
        XMLPlatformUtils::Initialize();
    } catch ( const XMLException& toCatch ) {
        string message = XMLHelper<string>::safeTranscode( toCatch.getMessage() );
        cout << "Error during initialization!"<< endl << message << endl;
        return -1;
    }

    DOMNode* root = 0;
    XercesDOMParser* parser = new XercesDOMParser();
    parser->setValidationScheme( XercesDOMParser::Val_Always );
    parser->setDoNamespaces( false );
    parser->setDoSchema( true );
    parser->setCreateCommentNodes( false ); // No comment nodes
    parser->setIncludeIgnorableWhitespace( false ); // No text nodes

    ErrorHandler* errHandler = ( ErrorHandler* ) new HandlerBase();
    parser->setErrorHandler( errHandler );
    // XML Parser initialized.

    // Initialize the LoggerFactory
    const string loggerFileName = string( __ROOT_PREFIX__ ) + string( "logger_factory.xml" );
    root = XMLHelper<void>::parseXML( loggerFileName, parser );
    LoggerFactory::XMLParse( root );

    // Parse configuration file.
    root = XMLHelper<void>::parseXML( configurationFileName, parser );
    conf->XMLParse( root );

    // Open various files.
    const string logFileName = conf->getFile( "logOutFileName" );
    logfile.open( logFileName.c_str(), ios::out );
    util::checkIsOpen( logfile, logFileName  );

    const string xmlOutFileName = conf->getFile( "xmlOutputFileName" );
    xmlOutStream.open( xmlOutFileName.c_str(), ios::out );
    util::checkIsOpen( xmlOutStream, xmlOutFileName );

    const string bugOutFileName = conf->getFile( "bugOutFileName" );
    bugoutfile.open( bugOutFileName.c_str(), ios::out );
    util::checkIsOpen( bugoutfile, bugOutFileName );

    const string outFileName = conf->getFile( "outFileName" );
    outfile.open( outFileName.c_str(), ios::out );
    util::checkIsOpen( outfile, outFileName );

    const string dbOutFileName = conf->getFile( "dbOutFileName" );
    dbout.open( dbOutFileName.c_str(), ios::out );
    util::checkIsOpen( dbout, dbOutFileName );


    root = XMLHelper<void>::parseXML( conf->getFile( "xmlInputFileName" ), parser );

    // Use a smart pointer for scenario so that if the main program exits before the end the memory is freed correctly. 
    auto_ptr<Scenario> scenarioPtr( new Scenario() );
    scenario = scenarioPtr.get();

    scenario->XMLParse( root );

    if( conf->getBool( "runningNonReference" ) ) {
        const int numAddFiles = conf->getInt( "NumberOfScenarioAddOnFiles" );
        cout << "Number of additional scenario files: " << numAddFiles << endl;
        for( int fileNum = 1; fileNum <= numAddFiles; fileNum++ ) {
            // Now read in scenario specific information.
            cout << "Reading in additional scenario file number: " << fileNum << "." << endl;
            stringstream fileNameStream;
            fileNameStream << "scenarioXmlInputFileName" << fileNum;
            string xmlFileName;
            fileNameStream >> xmlFileName;
            string addOnFileName = conf->getFile( xmlFileName );

            if( addOnFileName != "" ) {
                root = XMLHelper<void>::parseXML( addOnFileName, parser );
                scenario->XMLParse( root );
            }
        }  
    }

    cout << "XML parsing complete." << endl;
    logfile << "XML parsing complete." << endl;

    // Cleanup Xerces.
    delete errHandler;
    delete parser;
    XMLPlatformUtils::Terminate();

    // Finish initialization.
    scenario->completeInit();

    // Compute data read in time
    afterinit = clock();
    duration = (double)(afterinit-start) / CLOCKS_PER_SEC;
    cout << "XML Readin Time: " << duration << " Seconds" << endl;
    logfile << "XML Readin Time: " << duration << " Seconds" << endl;

    const Modeltime* modeltime = scenario->getModeltime();
    World* world = scenario->getWorld();
    const Marketplace* marketplace = scenario->getMarketplace();

    int t;
    outfile <<"Region,Sector,Subsector,Technology,Variable,Units,";

    for (t=0;t<modeltime->getmaxper();t++) { 
        outfile << modeltime->getper_to_yr(t) <<",";
    }
    outfile <<"Date,Notes" << endl;

    // output file header
    dbout <<"RunID,Region,VarID,";
    for (t=1;t<modeltime->getmaxper();t++) { 
        dbout<<"y" << modeltime->getper_to_yr(t) <<",";
    }
    dbout << endl;
    // ******* end MiniCAM stype output *******
 
    scenario->run();

    // Print output xml file.
    scenario->toXML( xmlOutStream );

    // compute data read in time
    duration = (double)(afterinit-start) / CLOCKS_PER_SEC;
    cout << endl << "Data Readin Time: "<<duration<<" Seconds" << endl;

    // compute model run time
    intermediate = clock();
    duration = (double)(intermediate-start) / CLOCKS_PER_SEC;
    cout << "Data Readin & Model Run Time: "<<duration<<" Seconds" << endl;

    if ( conf->getBool( "timestamp" )  ) {
        bugoutfile << endl << "Model Run Time: ,"<< duration <<", Seconds";
    }

    // ***** Write results to database after last period
    openDB(); // open MS Access database
    createDBout(); // create main database output table before calling output routines

    // ***** Write to text file and database
    world->outputfile(); // write results to file
    world->MCoutput(); // MiniCAM style output to database
    marketplace->MCoutput(); // write global market info to database
    createMCvarid(); // create MC variable id's 
    // ***** end of writing to database

    finish = clock(); 
    duration = (double)(finish-start) / CLOCKS_PER_SEC;
    logfile << "Data Readin, Model Run & Write Time: "<< duration <<" Seconds" << endl;
    cout << endl<< "Date & Time: "<<ctime(&ltime)<< endl;
    logfile << endl<< "Date & Time: "<< ctime( &ltime ) << endl;

    if ( conf->getBool( "timestamp" ) ) { 
        bugoutfile << endl<< "Total Run & Write Time: ,"<< duration <<", Seconds";
    }

    if( conf->getBool( "agSectorActive" ) ){
        AgSector::internalOutput();
    }

    //******** Close All Text Files
    xmlOutStream.close();
    outfile.close();
    bugoutfile.close();
    logfile.close();
    sdcurvefile.close();
    sdfile.close();
    dbout.close();
    closeDB(); // close MS Access database
    LoggerFactory::cleanUp();

    return 0;
}