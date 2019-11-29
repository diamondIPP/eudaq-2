// EUDAQ includes:
#include "eudaq/Producer.hh"
#include "eudaq/Logger.hh"
#include "eudaq/OptionParser.hh"

#include "DRS4Producer.hh"
#include "iostream"

// system includes:
#include <iostream>
#include <ostream>
#include <vector>
#include <mutex>
#include <cmath>
#include <string>
#include <unistd.h>
#include <algorithm>

using namespace std;
//static const std::string EVENT_TYPE = "DRS4";

DRS4Producer::DRS4Producer(const string & name, const string & runcontrol)  : eudaq::Producer(name, runcontrol),
    m_ev(0),
    n_samples(1024),
    m_event_type("DRS4RawDataEvent"),
    m_producer_name(name),
    m_tlu_waiting_time(4000),
    m_self_triggering(false),
    m_baseline_offset(0.),
    m_running(false),
    m_terminated(false),
    is_initalized(false),
    n_channels(4),
    m_board(nullptr) {

	cout << "Started DRS4Producer with Name: \"" << name << "\"" << endl;
  cout<<"Event Type:" << m_event_type << endl;
	cout << "Geting DRS() ..." << endl;
	m_drs = new DRS();
  m_n_boards = m_drs->GetNumberOfBoards();
  if (not m_n_boards)
    EUDAQ_ERROR(string("No Board connected exception."));
  cout <<"Found DRS: " << m_drs << " with " << m_n_boards << "boards" << endl;
  m_channel_active.resize(8, false);
}

namespace{
  auto dummy0 = eudaq::Factory<eudaq::Producer>::Register<DRS4Producer, const string&, const string&>(DRS4Producer::m_id_factory);
}

void DRS4Producer::DoConfigure() {
  cout << "Configure DRS4 board" << endl;
  auto config = GetConfiguration();
  while (!m_drs){
    cout << "wait for configure" << endl;
    usleep(10);
  }
  try {/** do initial scan */
    m_serialno = config->Get("serialno",-1);
    m_self_triggering = config->Get("self_triggering", false);
    m_n_self_trigger = config->Get("n_self_trigger",unsigned(1e5));
    /* show any found board(s) */
    uint16_t board_no = 0;
    for (auto i(0) ; i < m_n_boards ; i++) {
      DRSBoard * board = m_drs->GetBoard(i);
      printf("    #%2d: serial #%d, firmware revision %d\n", (int)i, board->GetBoardSerialNumber(), board->GetFirmwareVersion());
      if (board->GetBoardSerialNumber() == m_serialno)
        board_no = i;
    }//loop over number of boards
    cout << "Found board number: " << board_no << endl;
    if (m_board == nullptr){
      m_board = m_drs->GetBoard(board_no);
      cout << "Initialising DRS4 board " << endl;
      m_board->Init();
    } else {
      cout << "Reinitialising DRS4 board " << endl;
      m_board->Reinit();
    }

    double sampling_frequency = config->Get("sampling_frequency", 5);
    bool wait_for_PLL_lock = config->Get("wait_for_PLL_lock",true);
    cout << "Set sampling freqeuncy to: " << sampling_frequency << " | Wait for PLL lock" << wait_for_PLL_lock << endl;
    m_board->SetFrequency(sampling_frequency, wait_for_PLL_lock);
    m_board->SetTranspMode(1);  /** enable transparent mode needed for analog trigger */

    m_baseline_offset = config->Get("base_line_offset", float(0));
    if (abs(m_baseline_offset) > .5){
      EUDAQ_WARN("Baseline offset cannot be larger than .5 -> setting it to 0");
      m_baseline_offset = 0;
    }
    EUDAQ_INFO(string("Set Input Range to: " + to_string(m_baseline_offset - 0.5) + "V - " + to_string(m_baseline_offset - 0.5) + "V"));
    m_board->SetInputRange(m_baseline_offset);


    if (config->Get("enable_calibration_clock", false)) {
      cout << " turn on the internal 100 MHz clock connected to all channels " << endl;
      m_board->EnableTcal(1); /** Internal 100 MHz clock connected to all channels */
    }

    /** Internal trigger setup */
    if (m_board->GetBoardType() >= 8) {            /** Evaluation Board V4&5 */
      m_board->EnableTrigger(1, 0);           /** enable hardware trigger */
      //m_board->SetTriggerSource(1<<0);        // set CH1 as source
    } else if (m_board->GetBoardType() == 7) { /** Evaluation Board V3 */
      m_board->EnableTrigger(0, 1);           // lemo off, analog trigger on
      //m_board->SetTriggerSource(0);           /** use CH1 as source */
    }
    float trigger_level = config->Get("trigger_level", float(-0.05));  /** in Volt */
    uint32_t trigger_delay = config->Get("trigger_delay", 500);
    bool trigger_polarity = config->Get("trigger_polarity", false);
    m_board->SetTriggerLevel(trigger_level);
    m_board->SetTriggerDelayNs(trigger_delay);
    m_board->SetTriggerPolarity(trigger_polarity);
    EUDAQ_INFO(string("Set Trigger Level to: " + std::to_string(trigger_level) + " mV"));
    EUDAQ_INFO(string("Set Trigger Delay to: " + std::to_string(trigger_delay) + " ns"));
    EUDAQ_INFO(string("Set Trigger Polarity to" + string(trigger_polarity ? "positive" : "negative") + " edge"));

    int trigger_source = config->Get("trigger_source", 1);
    cout<<string("Set Trigger Source to: " + to_string(trigger_source)) << endl;
    EUDAQ_INFO(string("Set Trigger Source to: "+std::to_string(trigger_source)));
    m_board->SetTriggerSource(trigger_source);

    activated_channels = config->Get("activated_channels", 255);
    for (size_t i(0); i < m_channel_active.size(); i++){
      m_channel_active.at(i) = (activated_channels & (1 << i)) == 1 << i;
      cout << "CH" << i + 1 << ": " << (m_channel_active.at(i) ? "ON" : "OFF") << endl;
    }
//    SetStatus(eudaq::Status::STATE_CONF, "Configured (" + config->Name() + ")");

  } catch ( ... ){
    EUDAQ_ERROR(string("Unknown exception."));
    SetStatus(eudaq::Status::STATE_ERROR, "Unknown exception.");
  }
  cout << "DRS4 Configured! Ready to take data" << endl;
}

void DRS4Producer::DoStartRun() {
	m_ev = 0;
	try{
		this->SetTimeStamp();
		cout << "Start Run: " << GetRunNumber() << " @ " << m_timestamp << endl;
		cout << "Create raw event" << m_id_factory << endl;
		auto bore = eudaq::Event::MakeUnique(m_event_type);
		bore->SetTag("NChannels", n_channels);
		bore->SetTag("SerialNumber", m_serialno);
		bore->SetTag("Firmware", to_string(m_board->GetFirmwareVersion()));
    bore->SetTag("BoardType",to_string(m_board->GetBoardType()));
		bore->SetTag("ActiveChannels",to_string(activated_channels));
		bore->SetTag("DeviceName","DRS4");
    bore->SetTag("BaselineOffset",to_string(m_baseline_offset));
    uint16_t wave_depth = m_board->GetChannelDepth();
    bore->SetTag("WaveDepth", to_string(wave_depth));
    EUDAQ_INFO(string("DRS4 Wavedepth: " + std::to_string(wave_depth)));

    uint16_t block_id = 0;
		for (auto ch(0); ch < n_channels; ch++){
      string tag = "CH_" + to_string(ch + 1);
		  bore->SetTag(tag, m_channel_active.at(ch) ? GetConfiguration()->Get(tag, "OFF") : "OFF");  /** Set Names for different channels */
			cout << "\tBore tag: " << tag << "\t" << bore->GetTag(tag) << endl;
      if (m_channel_active.at(ch)){
        float tcal[2048];
        m_board->GetTimeCalibration(0, ch * 2, 0, tcal, false);
        if (wave_depth == 2048)
          for (int j=0 ; j < wave_depth ; j+=2)
            tcal[j/2] = (tcal[j]+tcal[j+1]) / 2;
        vector<float> v_tcal(tcal, tcal + sizeof(tcal) / sizeof(tcal[0]));
        bore->AddBlock(block_id, v_tcal);
		  }
		}

		cout << "Send BORE event" << endl;
    bore->SetBORE();
		SendEvent(move(bore));

//		SetStatus(eudaq::Status::STATE_RUNNING , "Running");
		m_running = true;
		/* Start Readout */
		if (m_board)
			m_board->StartDomino();
	} catch (...) {
		EUDAQ_ERROR(string("Unknown exception."));
		SetStatus(eudaq::Status::STATE_ERROR, "Unknown exception.");
	}
}

void DRS4Producer::DoStopRun() {
  /** Wait before we stop the DAQ because TLU takes some time to pick up the OnRunStop signal otherwise the last triggers get lost. */
  eudaq::mSleep(m_tlu_waiting_time);
	m_running = false;
	cout << "Run stopped." << endl;
	try {
    /** Stop DRS Board */
		if (m_board && m_board->IsBusy()) {
      m_board->SoftTrigger();
			for (int i=0 ; i<10 and m_board->IsBusy(); i++)
				usleep(10); //todo not mt save
		}
    /** Sending the final end-of-run event */
    auto eore = eudaq::Event::MakeUnique(m_event_type);
    eore->SetEORE();
    SendEvent(move(eore));

    std::cout << "RUN " << GetRunNumber() << " DRS4 " << endl << "\t Total triggers:   \t" << m_ev << endl;
    EUDAQ_USER(string("Run " + to_string(GetRunNumber()) + ", ended with " + to_string(m_ev) + " Events"));

//		SetStatus(eudaq::Status::STATE_STOPPED, "Stopped");
	} catch ( ... ){
		EUDAQ_ERROR(string("Unknown exception."));
		SetStatus(eudaq::Status::STATE_ERROR, "Unknown exception.");
	}
}

void DRS4Producer::DoTerminate() {
  
	m_terminated = true;
  if (m_drs) {
    delete m_drs;
    m_drs = nullptr;
  }
  cout << "DRS4Producer " << m_producer_name << " terminated." << endl;
}

void DRS4Producer::RunLoop() {

  cout << "Start ReadoutLoop " << m_terminated << endl;
	int k = 0;
	while (m_running) {
    if (!m_board)
      continue;
    k++;
    /** Check if ready for triggers */
    if (m_board->IsBusy()){
      if (m_self_triggering && k % int(1e4) == 0 ){
        cout << "Send software trigger" << endl;
        m_board->SoftTrigger();
        usleep(10); //todo not mt save
      } else {
        continue;
      }
    }
    /** Trying to get the next event, daqGetRawEvent throws exception if none is available: */
    try {
      SendRawEvent();
      cout << "DRS4 Event number: " << m_ev << endl;
      if (m_ev % 1000 == 0)
        cout << "DRS4 Board " << " EVT " << m_ev << endl;
    } catch(int e) {
      /** No event available, return to scheduler: */
      cout << "An exception occurred. Exception Nr. " << e << '\n';
      sched_yield();
    }
  }
	cout << "ReadoutLoop Done." << endl;
}

void DRS4Producer::SendRawEvent() {

  auto event = eudaq::Event::MakeUnique(m_event_type);
  SetTimeStamp();
  /* read all waveforms */
  cout << " BLAAAAAAAAAAAAAAAAAAAAAAAAA" << m_board->GetNumberOfChannels() << " " <<m_board->GetNumberOfChips() << endl;
  m_board->TransferWaves(0, 8);

  /** read time (X) array of channel in ns
      Note: On the evaluation board input #1 is connected to channel 0 and 1 of the DRS chip,
      input #2 is connected to channel 2 and 3 and so on. So to get the input #2 we have to read DRS channel #2, not #1. */
  for (int ch = 0; ch < n_channels; ch++)
    m_board->GetWave(0, ch * 2, wave_array[ch]);
  int trigger_cell = m_board->GetTriggerCell(0);

  m_board->StartDomino(); /** Restart Readout */
  uint16_t block_no = 0;
  event->AddBlock(block_no++, reinterpret_cast<const char*>(&trigger_cell), sizeof(trigger_cell));
  event->AddBlock(block_no++, reinterpret_cast<const char*>(&m_timestamp), sizeof(m_timestamp));
  for (int ch = 0; ch < n_channels; ch++){
    if (not m_channel_active.at(ch))
      continue;
    char buffer [6];
    sprintf(buffer, "C%03d\n", ch+1);
    event->AddBlock(block_no++, reinterpret_cast<const char*>(buffer), sizeof(buffer));
    /* Set data block of ch, each channel ist connected to to DRS channels*/
    unsigned short raw_wave_array[n_samples];
    for (uint16_t i(0); i < n_samples; i++)
      raw_wave_array[i] = (unsigned short)((wave_array[ch][i] / 1000.0 - m_baseline_offset + 0.5) * 65535);
    event->AddBlock(block_no++, reinterpret_cast<const char*>(&raw_wave_array), sizeof(raw_wave_array[0]) * n_samples);
  }
  SendEvent(move(event));
  m_ev++;
}

void DRS4Producer::SetTimeStamp() {

  std::chrono::high_resolution_clock::time_point epoch;
  auto now = std::chrono::high_resolution_clock::now();
  auto elapsed = now - epoch;
  m_timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count() / 100u;
}
