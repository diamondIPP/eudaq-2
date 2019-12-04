#include "eudaq/Producer.hh"
#include "eudaq/Logger.hh"
#include "eudaq/Time.hh"
#include "eudaq/Utils.hh"
#include "eudaq/OptionParser.hh"
#include "eudaq/Configuration.hh"
#include "RConfigure.h" // Version symbols

#include "api.h"
#include "constants.h"
#include "dictionaries.h"
#include "log.h"
#include "helper.h"

#include "CMSPixelProducer.hh"

#include <iostream>
#include <ostream>
#include <vector>
#include <sched.h>

using namespace pxar;
using namespace std;

CMSPixelProducer::CMSPixelProducer(const string & name, const string & runcontrol)
  : eudaq::Producer(name, runcontrol),
    m_ev(0),
    m_ev_filled(0),
    m_ev_runningavg_filled(0),
    m_tlu_waiting_time(4000),
    m_roc_resetperiod(0),
    m_nplanes(1),
    m_terminated(false),
    m_running(false),
    m_api(nullptr),
    m_trimmingFromConf(false),
    m_pattern_delay(0),
    m_trigger_is_pg(false),
    m_fout(nullptr),
    m_foutName(""),
    triggering(false),
    m_roctype(""),
    m_pcbtype(""),
    m_usbId(""),
    m_producer_name(name),
    m_detector(""),
    m_event_type(""),
    m_alldacs(""),
    m_last_mask_filename(""),
    m_start_time(chrono::steady_clock::now())
{
  if(m_producer_name.find("REF") != string::npos) {
    m_detector = "REF";
    m_event_type = "CMSPixelREF";
  } else if(m_producer_name.find("TRP") != string::npos) {
    m_detector = "TRP";
    m_event_type = "CMSPixelTRP";
  } else if(m_producer_name.find("ANA") != string::npos) {
    m_detector = "ANA";
    m_event_type = "CMSPixelANA";
  } else if(m_producer_name.find("DIG") != string::npos) {
    m_detector = "DIG";
    m_event_type = "CMSPixelIDIG";
  } else {
    m_detector = "DUT";
    m_event_type = "CMSPixelDUT";
  }
}

namespace{
  auto dummy0 = eudaq::Factory<eudaq::Producer>::Register<CMSPixelProducer, const string&, const string&>(CMSPixelProducer::m_id_factory0);
  auto dummy1 = eudaq::Factory<eudaq::Producer>::Register<CMSPixelProducer, const string&, const string&>(CMSPixelProducer::m_id_factory1);
  auto dummy2 = eudaq::Factory<eudaq::Producer>::Register<CMSPixelProducer, const string&, const string&>(CMSPixelProducer::m_id_factory2);
  auto dummy3 = eudaq::Factory<eudaq::Producer>::Register<CMSPixelProducer, const string&, const string&>(CMSPixelProducer::m_id_factory3);
  auto dummy4 = eudaq::Factory<eudaq::Producer>::Register<CMSPixelProducer, const string&, const string&>(CMSPixelProducer::m_id_factory4);
}

void CMSPixelProducer::DoConfigure() {

  m_config = GetConfiguration();
  cout << "Configuring: " << m_config->Name() << endl;
  bool confTrimming(false), confDacs(false);

  uint8_t hubid = m_config->Get("hubid", 31);
  string value = m_config->Get("event_type", "INVALID");
  transform(value.begin(), value.end(), value.begin(), ::tolower);
  if (value.find("dut") != string::npos)
    m_event_type = "CMSPixelDUT";
  else if (value.find("ref") != string::npos)
    m_event_type = "CMSPixelREF";
  else if (value.find("trp") != string::npos)
    m_event_type = "CMSPixelTRP";
  else if (value.find("analog") != string::npos)
    m_event_type = "CMSPixelANA";
  else if (value.find("digital") != string::npos)
    m_event_type = "CMSPixelDIG";
  EUDAQ_INFO(string("Set Event Type of " + m_producer_name + " to " + m_event_type + "."));
  m_detector = m_config->Get("detector_name", "INVALID");
  EUDAQ_INFO(string("Set Detector Name to \"" + m_detector + "\"."));

  // Store waiting time in ms before the DAQ is stopped in OnRunStop():
  m_tlu_waiting_time = m_config->Get("tlu_waiting_time", 4000);
  EUDAQ_INFO(string("Waiting " + to_string(m_tlu_waiting_time) + "ms before stopping DAQ after run stop."));

  // DTB delays
  vector<pair<string, uint8_t> > sig_delays;
  sig_delays.emplace_back(make_pair("clk", m_config->Get("clk", 4)));
  sig_delays.emplace_back(make_pair("ctr", m_config->Get("ctr", 4)));
  sig_delays.emplace_back(make_pair("sda", m_config->Get("sda", 19)));
  sig_delays.emplace_back(make_pair("tin", m_config->Get("tin", 9)));
  sig_delays.emplace_back(make_pair("deser160phase", m_config->Get("deser160phase", 4)));
  sig_delays.emplace_back(make_pair("level", m_config->Get("level", 15)));
  sig_delays.emplace_back(make_pair("triggerlatency", m_config->Get("triggerlatency", 80)));
  sig_delays.emplace_back(make_pair("tindelay", m_config->Get("tindelay", 13)));
  sig_delays.emplace_back(make_pair("toutdelay", m_config->Get("toutdelay", 10)));
  sig_delays.emplace_back(make_pair("triggertimeout", m_config->Get("triggertimeout", 65000)));
  //Power settings:
  vector<pair<string, double> > power_settings;
  power_settings.emplace_back( make_pair("va",m_config->Get("va",1.8)) );
  power_settings.emplace_back( make_pair("vd",m_config->Get("vd",2.5)) );
  power_settings.emplace_back( make_pair("ia",m_config->Get("ia",1.10)) );
  power_settings.emplace_back( make_pair("id",m_config->Get("id",1.10)) );

  // Periodic ROC resets:
  m_roc_resetperiod = m_config->Get("rocresetperiod", 0);
  if(m_roc_resetperiod > 0) { EUDAQ_USER("Sending periodic ROC resets every " + eudaq::to_string(m_roc_resetperiod) + "ms\n"); }

  // Pattern Generator:
  vector<pair<string, uint8_t> > pg_setup;
  bool testpulses = m_config->Get("testpulses", false);
  if(testpulses) {
    uint16_t pgcal = m_config->Get("wbc", uint16_t(100));
    pgcal += m_roctype.find("dig") ? 6 : 5;
    pg_setup.emplace_back(make_pair("resetroc",m_config->Get("resetroc", 25)) );
    pg_setup.emplace_back(make_pair("calibrate",m_config->Get("calibrate", 106)) );
    pg_setup.emplace_back(make_pair("trigger",m_config->Get("trigger", 16)) );
    pg_setup.emplace_back(make_pair("token",m_config->Get("token", 0)));
    m_pattern_delay = m_config->Get("patternDelay", 100) * 10;
    EUDAQ_USER("Using testpulses, pattern delay " + eudaq::to_string(m_pattern_delay) + "\n");
  } else {
    pg_setup.emplace_back(make_pair("trigger", 46));
    pg_setup.emplace_back(make_pair("token", 0));
    m_pattern_delay = m_config->Get("patternDelay", 100);
  }
  try {
    // Check for multiple ROCs using the I2C parameter:
    vector<int32_t> i2c_addresses = split(m_config->Get("i2c", "i2caddresses", "-1"), ' ');
    cout << "Found " << i2c_addresses.size() << " I2C addresses: " << pxar::listVector(i2c_addresses) << endl;

    // Set the type of the TBM and read registers if any:
    vector<vector<pair<string, uint8_t> > > tbmDACs;
    m_tbmtype = m_config->Get("tbmtype", "notbm");
    try {
      tbmDACs.push_back(GetConfDACs(0, true));
      tbmDACs.push_back(GetConfDACs(1, true));
    }
    catch(pxar::InvalidConfig&) {}

    // Set the type of the ROC correctly:
    m_roctype = m_config->Get("roctype", "psi46digv21respin");
    
    /** Set different wbcs */
    vector<int32_t> wbc_values = split(m_config->Get("wbc", "wbcaddresses", "-1"), ' ');
    cout << "WBC ADDRESSES:" << endl;
    for (auto wbc: wbc_values)
      cout << "  " << wbc << endl;

    // Read the type of carrier PCB used ("desytb", "desytb-rot"):
    m_pcbtype = m_config->Get("pcbtype", "desytb");

    // Read the mask file if existent:
    vector<pxar::pixelConfig> maskbits = GetConfMaskBits();

    // Read DACs and Trim settings for all ROCs, one for each I2C address:
    vector<vector<pair<string, uint8_t> > > rocDACs;
    vector<vector<pxar::pixelConfig> > rocPixels;
    vector<uint8_t> rocI2C;
    for(int32_t i2c : i2c_addresses) {
      // Read trim bits from m_config:
      rocPixels.push_back(GetConfTrimming(maskbits,static_cast<int16_t>(i2c)));
      // Read the DAC file and update the vector with overwrite DAC settings from m_config:
      rocDACs.push_back(GetConfDACs(static_cast<int16_t>(i2c)));
      // Add the I2C address to the vector:
      if(i2c > -1) { rocI2C.push_back(static_cast<uint8_t>(i2c)); }
      else { rocI2C.push_back(static_cast<uint8_t>(0)); }
    }

    // Set the type of the ROC correctly:
    m_roctype = m_config->Get("roctype","psi46digv2");

    // Read the type of carrier PCB used ("desytb", "desytb-rot"):
    m_pcbtype = m_config->Get("pcbtype","desytb");

    /** create api */
    if (m_api)
      delete m_api;
    m_usbId = m_config->Get("usbId", "*");
    EUDAQ_EXTRA("Trying to connect to USB id: " + m_usbId + "\n");
    m_api = new pxar::pxarCore(m_usbId, m_verbosity);

    // Initialize the testboard:
    if(!m_api->initTestboard(sig_delays, power_settings, pg_setup)) {
      EUDAQ_ERROR(string("Firmware mismatch."));
      throw pxar::pxarException("Firmware mismatch");
    }

    // Initialize the DUT as m_configured above:
    m_api->initDUT(hubid,m_tbmtype, tbmDACs, m_roctype, rocDACs, rocPixels, rocI2C);
    // Store the number of m_configured ROCs to be stored in a BORE tag:
    m_nplanes = rocDACs.size();

    // Read current:
    cout << "Analog current: " << m_api->getTBia() * 1000 << "mA" << endl;
    cout << "Digital current: " << m_api->getTBid() * 1000 << "mA" << endl;

    if(m_api->getTBid()*1000 < 15) EUDAQ_ERROR(string("Digital current too low: " + to_string(1000*m_api->getTBid()) + "mA"));
    else EUDAQ_INFO(string("Digital current: " + to_string(1000*m_api->getTBid()) + "mA"));

    if(m_api->getTBia()*1000 < 15) EUDAQ_ERROR(string("Analog current too low: " + to_string(1000*m_api->getTBia()) + "mA"));
    else EUDAQ_INFO(string("Analog current: " + to_string(1000*m_api->getTBia()) + "mA"));

    // Send a single RESET to the ROC to initialize its status:
    if(!m_api->daqSingleSignal("resetroc")) { throw InvalidConfig("Unable to send ROC reset signal!"); }

    // Switching to external clock if requested and check if DTB returns TRUE status:
    cout << "external clock: " << m_config->Get("extclock", 1) << endl;
    if(!m_api->setExternalClock(m_config->Get("extclock", 1) != 0)) {
      throw InvalidConfig("Couldn't switch to " + string(m_config->Get("extclock",1) != 0 ? "external" : "internal") + " clock.");
    }
    else {
      EUDAQ_INFO(string("Clock set to " + string(m_config->Get("extclock", 1) != 0 ? "external" : "internal")));
    }

    // Switching to the selected trigger source and check if DTB returns TRUE:
    string triggersrc = m_config->Get("trigger_source","extern");
    if(!m_api->daqTriggerSource(triggersrc)) {
      throw InvalidConfig("Couldn't select trigger source " + string(triggersrc));
    }
    else {
      // Update the TBM setting according to the selected trigger source.
      // Switches to TBM_EMU if we selected a trigger source using the TBM EMU.
      if(m_tbmtype == "notbm" && pxar::TriggerDictionary::getInstance()->getEmulationState(triggersrc))
	      m_tbmtype = "tbmemulator";
      if(triggersrc == "pg" || triggersrc == "pg_dir" || triggersrc == "patterngenerator")
	      m_trigger_is_pg = true;
      EUDAQ_INFO(string("Trigger source selected: " + triggersrc));
    }

    // Output the m_configured signal to the probes:
    string signal_d1 = m_config->Get("signalprobe_d1", "off");
    string signal_d2 = m_config->Get("signalprobe_d2", "off");
    string signal_a1 = m_config->Get("signalprobe_a1", "off");
    string signal_a2 = m_config->Get("signalprobe_a2", "off");

    if(m_api->SignalProbe("d1", signal_d1) && signal_d1 != "off") {
      EUDAQ_EXTRA("Setting scope output D1 to \"" + signal_d1 + "\"\n");
    }
    if(m_api->SignalProbe("d2", signal_d2) && signal_d2 != "off") {
      EUDAQ_EXTRA("Setting scope output D2 to \"" + signal_d2 + "\"\n");
    }
    if(m_api->SignalProbe("a1", signal_a1) && signal_a1 != "off") {
      EUDAQ_EXTRA("Setting scope output A1 to \"" + signal_a1 + "\"\n");
    }
    if(m_api->SignalProbe("a2", signal_a2) && signal_a2 != "off") {
      EUDAQ_EXTRA("Setting scope output A2 to \"" + signal_a2 + "\"\n");
    }

    EUDAQ_EXTRA(m_api->getVersion() + string(" API set up successfully...\n"));

    // test pixels
    if(testpulses) {
      cout << "Setting up pixels for calibrate pulses..." << endl << "col \t row" << endl;
      for(int i = 40; i < 45; i++)
	      m_api->_dut->testPixel(25,i,true);
    }

    /**MASKING*/
    vector<masking> pixel_masks = GetConfMask();

    /**mask whole ROC for the antimask identifier*/
    for (auto & pixel_mask : pixel_masks){
        if (pixel_mask.id()[0] != '#' && pixel_mask.id()[0] != ' ' && !pixel_mask.id().empty()){
            uint16_t roc = pixel_mask.roc();
            if (pixel_mask.id() == "cornBot") {
                m_api->_dut->maskAllPixels(true, roc);
                cout << "masking whole ROC " << roc << "\n";
            }
        }
    }

    for (uint16_t i(0); i<pixel_masks.size(); i++){
        if (pixel_masks[i].id()[0] != '#' && pixel_masks[i].id()[0] != ' ' && !pixel_masks[i].id().empty()){
            uint16_t col = pixel_masks[i].col(); string cCol = to_string(col/16)+char(col%16 < 10 ? col%16+'0' : col%16+'W');
            uint16_t row = pixel_masks[i].row(); string cRow = to_string(row/16)+char(row%16 < 10 ? row%16+'0' : row%16+'W');
            uint16_t roc = pixel_masks[i].roc(); char cRoc = (roc < 10 ? roc+'0' : roc+'W');
            string sCol = cRoc+cCol, sRow = "00"+cRow;

            if (pixel_masks[i].id() == "pix")
              m_api->_dut->maskPixel(col, row, true, roc);
            else if (pixel_masks[i].id() == "col")
              for (uint16_t nCols(col); nCols < row+1; nCols++)
                for (uint16_t nRows(0); nRows < 80; nRows++)
                  m_api->_dut->maskPixel(nCols, nRows, true, roc);
            else if (pixel_masks[i].id() == "row")
              for (uint16_t nRows(col); nRows < row+1; nRows++)
                for (uint16_t nCols(0); nCols < 52; nCols++)
                  m_api->_dut->maskPixel(nCols, nRows, true, roc);
            else if (pixel_masks[i].id() == "roc"){
              cout << "masked roc "<< roc <<"\n";
              EUDAQ_INFO(string("ROC ") + string(to_string(roc)) + string(" fully masked"));
              m_api->_dut->maskAllPixels(true,roc);
            } //end elseif

            /**antimask*/
            else if (pixel_masks[i].id() == "cornBot" && pixel_masks[i+1].id() == "cornTop"){
                uint16_t row_start = pixel_masks[i].row();   string cRow1 = to_string(row_start/16)+char(row_start%16 < 10 ? row_start%16+'0' : row_start%16+'W');
                uint16_t row_stop = pixel_masks[i+1].row();  string cRow2 = to_string(row_stop/16)+char(row_stop%16 < 10 ? row_stop%16+'0' : row_stop%16+'W');
                uint16_t col_start = pixel_masks[i].col();   string cCol1 = to_string(col_start/16)+char(col_start%16 < 10 ? col_start%16+'0' : col_start%16+'W');
                uint16_t col_stop = pixel_masks[i+1].col();  string cCol2 = to_string(col_stop/16)+char(col_stop%16 < 10 ? col_stop%16+'0' : col_stop%16+'W');
                string sCol1 = cRoc+cCol1, sCol2 = "00"+cCol2, sRow1 = "00"+cRow1, sRow2 = "00"+cRow2;
                for (uint16_t nRows(row_start); nRows <= row_stop; nRows++){
                    for (uint16_t nCols(col_start); nCols <= col_stop; nCols++) {
                        m_api->_dut->maskPixel(nCols, nRows, false, roc);
                        }}
                cout << "unmasked box from col "<< col_start <<" to " << col_stop;
                cout << " and row " << row_start << " to " << row_stop << " of ROC" << roc <<"\n";} //end elseif
            else if (pixel_masks[i].id() == "cornTop") ; //catch identifier to avoid the error message

            else {
                cout << "Wrong identifier in maskFile!\n";
                EUDAQ_WARN("Wrong identifier in maskFile!");}//end else
        }//end first if
    }//end of loop

    /**print the name of the maskfile*/
    string filename = m_config->Get("maskFile", "");
    m_last_mask_filename = filename;
    stringstream maskname; maskname << "Using maskfile " << filename;
    EUDAQ_INFO(maskname.str());

    /**print number of masked pixels on each ROC*/
    for (uint16_t roc(0); roc<m_api->_dut->getNEnabledRocs();roc++)
      cout << "--> masked " << m_api->_dut->getNMaskedPixels(roc) << " Pixels in ROC " << roc << "!\n";
    // Read DUT info, should print above filled information:
    m_api->_dut->info();

    cout << "Current DAC settings:" << endl;
    m_api->_dut->printDACs(0);

    if(!m_trimmingFromConf)
      EUDAQ_WARN( "Couldn't read trim parameters from \"" + m_config->Name() + "\".");
    cout << "=============================\nCONFIGURED\n=============================" << endl;
  } catch (pxar::InvalidConfig &e){
    EUDAQ_ERROR(string("Invalid m_configuration settings: " + string(e.what())));
    SetStatus(eudaq::Status::STATE_ERROR, string("Invalid m_configuration settings: ") + e.what());
    delete m_api;
  }
  catch (pxar::pxarException &e){
    EUDAQ_ERROR(string("pxarCore Error: " + string(e.what())));
    SetStatus(eudaq::Status::STATE_ERROR, string("pxarCore Error: ") + e.what());
    delete m_api;
  }
  catch (...) {
    EUDAQ_ERROR(string("Unknown exception."));
    SetStatus(eudaq::Status::STATE_ERROR, "Unknown exception.");
    delete m_api;
  }
}

/**STARTRUN*/
void CMSPixelProducer::DoStartRun() {
  m_ev = 0;
  m_ev_filled = 0;
  m_ev_runningavg_filled = 0;

  try {
    EUDAQ_INFO("Switching Sensor Bias HV ON.");
    EUDAQ_INFO(m_last_mask_filename);
    m_api->HVon();

    cout << "Start Run: " << GetRunNumber() << endl;

    auto bore = eudaq::Event::MakeUnique(m_event_type);
    bore->SetBORE();

    bore->SetTag("ROCTYPE", m_roctype);
    bore->SetTag("TBMTYPE", m_tbmtype);
    bore->SetTag("MASKFILE",m_last_mask_filename);
    bore->SetTag("PLANES", m_nplanes);
    bore->SetTag("DACS", m_alldacs);
    bore->SetTag("PCBTYPE", m_pcbtype);
    bore->SetTag("DETECTOR", m_detector);
    bore->SetTag("PXARCORE", m_api->getVersion());
    SendEvent(move(bore));

    cout << "BORE with detector " << m_detector << " (event type " << m_event_type << ") and ROC type " << m_roctype << endl;

    m_api -> daqStart(); /** Start the Data Acquisition: */
    // Send additional ROC Reset signal at run start:
    if(!m_api->daqSingleSignal("resetroc"))
      throw InvalidConfig("Unable to send ROC reset signal!");
    else
      EUDAQ_INFO(string("ROC Reset signal issued."));

    /** If we run on Pattern Generator, activate the PG loop: */
    if(m_trigger_is_pg) {
      m_api -> daqTriggerLoop(m_pattern_delay);
      triggering = true;
    }

    // Start the timer for period ROC reset:
    m_start_time = chrono::steady_clock::now();
    m_running = true;
  }
  catch (...) {
    EUDAQ_ERROR(string("Unknown exception."));
    SetStatus(eudaq::Status::STATE_ERROR, "Unknown exception.");
    delete m_api;
  }
}

// This gets called whenever a run is stopped
void CMSPixelProducer::DoStopRun() {
  m_running = false;
  cout << "Stopping Run" << endl;

  try {
    // Acquire lock for pxarCore instance:
    lock_guard<mutex> lck(m_mutex);

    // If running with PG, halt the loop:
    if(m_trigger_is_pg) {
      m_api->daqTriggerLoopHalt();
      triggering = false;
    }

    // Wait before we stop the DAQ because TLU takes some time to pick up the OnRunStop signal
    // otherwise the last triggers get lost.
    eudaq::mSleep(m_tlu_waiting_time);

    m_api->daqStop(); /** Stop the Data Acquisition: */

    EUDAQ_INFO("Switching Sensor Bias HV OFF.");
    m_api->HVoff();

    try {
      // Read the rest of events from DTB buffer:
      vector<pxar::rawEvent> daqEvents = m_api->daqGetRawEventBuffer();
      cout << "CMSPixel " << m_detector << " Post run read-out, sending " << daqEvents.size() << " evt." << endl;
      for(auto & daqEvent : daqEvents) {
        auto event = eudaq::Event::MakeUnique(m_event_type);
	      event->AddBlock(0, daqEvent.data);
	      SendEvent(move(event));
	      if(daqEvent.data.size() > 1) { m_ev_filled++; }
	      m_ev++;
      }
    }
    catch(pxar::DataNoEvent &) {
      // No event available in derandomize buffers (DTB RAM),
    }

    // Sending the final end-of-run event:
    auto eore = eudaq::Event::MakeUnique(m_event_type);
    eore->SetEORE();
    cout << "CMSPixel " << m_detector << " Post run read-out finished." << endl;
    cout << "Stopped" << endl;

    // Output information for the logbook:
    cout << "RUN " << m_run << " CMSPixel " << m_detector << endl << "\t Total triggers:   \t" << m_ev << endl << "\t Total filled evt: \t" << m_ev_filled << endl;
    cout << "\t " << m_detector << " yield: \t" << (m_ev > 0 ? to_string(100*m_ev_filled/m_ev) : "(inf)") << "%" << endl;
    EUDAQ_USER(string("Run " + to_string(m_run) + ", detector " + m_detector + " yield: " + (m_ev > 0 ? to_string(100*m_ev_filled/m_ev) : "(inf)")
		      + "% (" + to_string(m_ev_filled) + "/" + to_string(m_ev) + ")"));

  } catch (const exception & e) {
    printf("While Stopping: Caught exception: %s\n", e.what());
    SetStatus(eudaq::Status::STATE_ERROR, "Stop Error");
  } catch (...) {
    printf("While Stopping: Unknown exception\n");
    SetStatus(eudaq::Status::STATE_ERROR, "Stop Error");
  }
}

void CMSPixelProducer::DoTerminate() {

  cout << "CMSPixelProducer terminating..." << endl;
  m_terminated = true;
  delete m_api;
  cout << "CMSPixelProducer " << m_producer_name << " terminated." << endl;
}

void CMSPixelProducer::RunLoop() {
  // Loop until Run Control tells us to terminate
  while (m_running) {

    // Send periodic ROC Reset
    if(m_roc_resetperiod > 0 && GetTime() > m_roc_resetperiod) {
      if(!m_api->daqSingleSignal("resetroc")) { EUDAQ_ERROR(string("Unable to send ROC reset signal!\n")); }
      ResetTime();
      }

    // Trying to get the next event, daqGetRawEvent throws exception if none is available:
    try {
	    pxar::rawEvent daqEvent = m_api->daqGetRawEvent();
	    auto event = eudaq::Event::MakeUnique(m_event_type);
      event->AddBlock(0, reinterpret_cast<const char*>(&daqEvent.data[0]), sizeof(daqEvent.data[0]) * daqEvent.data.size());
//	    event->AddBlock(0, daqEvent.data);
	    SendEvent(move(event));
      cout << "Pixel event: " << m_ev + 1 << ", hits: " << (daqEvent.GetSize() - 4 - m_nplanes * 2) / 6 << endl;
	    m_ev++;
      // Events with pixel data have more than 4 words for TBM header/trailer and 1 for each ROC header:
      if (daqEvent.data.size() > (4 + 3 * m_nplanes)) { m_ev_filled++; m_ev_runningavg_filled++; }

      // Print every 1k evt:
      if(m_ev%1000 == 0) {
        uint8_t filllevel = 0;
        m_api->daqStatus(filllevel);
        cout << "CMSPixel " << m_detector
            << " EVT " << m_ev << " / " << m_ev_filled << " w/ px" << endl;
        cout << "\t Total average:  \t" << (m_ev > 0 ? to_string(100*m_ev_filled/m_ev) : "(inf)") << "%" << endl;
        cout << "\t 1k Trg average: \t" << (100*m_ev_runningavg_filled/1000) << "%" << endl;
        cout << "\t RAM fill level: \t" << static_cast<int>(filllevel) << "%" << endl;
        m_ev_runningavg_filled = 0;
      }
    } catch(pxar::DataNoEvent &) {
    // No event available in derandomize buffers (DTB RAM), return to scheduler:
    sched_yield();
    }
  }
}


