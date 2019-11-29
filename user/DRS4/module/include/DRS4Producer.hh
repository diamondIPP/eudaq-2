#ifndef DRS4PRODUCER_HH
#define DRS4PRODUCER_HH

#include "eudaq/Producer.hh"
#include "eudaq/Time.hh"

#include "DRS.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <csignal>
#include <cctype>
#include <ctime>
#include <map>
#include <vector>
#include <ctime>
#include <chrono>
#include "strlcpy.h"


class DRS4Producer: public eudaq::Producer  {
public:
	DRS4Producer(const std::string & name, const std::string & runcontrol);
	void DoConfigure() override ;
	void DoStartRun() override ;
	void DoStopRun() override ;
	void DoTerminate() override ;
	void RunLoop() override ;

  static const uint32_t m_id_factory = eudaq::cstr2hash("DRS4Producer");

private:
	virtual void SendRawEvent();
	void SetTimeStamp();
  DRS * m_drs;
  DRSBoard * m_board;
  uint16_t m_n_boards;

  /** Configuration */
  int m_serialno;
  unsigned m_n_self_trigger;
  uint16_t n_samples;
  bool m_self_triggering;
  std::vector<bool> m_channel_active;
  uint8_t activated_channels;
  float m_baseline_offset;

	unsigned m_ev;
	unsigned m_tlu_waiting_time;
	std::string m_event_type, m_producer_name;
	bool m_terminated, m_running, triggering;
	std::uint64_t m_timestamp;
	bool is_initalized;
	float wave_array[8][1024];
	int n_channels;
	std::map<int, std::string> names;
};
int main(int /*argc*/, const char ** argv);
#endif /*DRS4PRODUCER_HH*/
