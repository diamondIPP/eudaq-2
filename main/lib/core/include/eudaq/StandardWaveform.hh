#ifndef EUDAQ_INCLUDED_StandardWaveform
#define EUDAQ_INCLUDED_StandardWaveform

#include "eudaq/Serializable.hh"
#include "eudaq/Serializer.hh"
#include "eudaq/Deserializer.hh"
#include "eudaq/Utils.hh"
#include "eudaq/Platform.hh"

#include <vector>
#include <string>

//#include "TF1.h"
class TF1;

namespace eudaq {

  class DLLEXPORT StandardWaveform : public Serializable {
  public:
      StandardWaveform(unsigned id, const std::string & type, const std::string & sensor = "");
      StandardWaveform(Deserializer &);
      StandardWaveform();
      void Serialize(Serializer &) const;
      void SetNSamples(unsigned n_samples);
      uint16_t GetNSamples() const {return m_n_samples;}
      template <typename T>
      void SetWaveform(T (*data)) {//todo: FIx issue with template
        m_samples.clear();
        for (size_t i = 0; i < m_n_samples;i++)
          m_samples.push_back(data[i]);
      } //todo: FIx issue with template
//	void SetWaveform(float* data);
      std::vector<float>* GetData() const{return &m_samples;};
      void SetTriggerCell(uint16_t trigger_cell) {m_trigger_cell=trigger_cell;}
      uint16_t GetTriggerCell() const{return m_trigger_cell;}
      void SetPolarities(signed char polarity, signed char pulser_polarity) { m_polarity = polarity; m_pulser_polarity = pulser_polarity; }
      void SetTimes(std::vector<float> * tcal) { m_times = getCalibratedTimes(tcal); }
      unsigned ID() const;
      void Print(std::ostream &) const;
      std::string GetType() const {return m_type;}
      std::string GetSensor() const {return m_sensor;}
      std::string GetChannelName() const {return m_channelname;};
      void SetChannelName(std::string channelname){m_channelname = channelname;}
      int GetChannelNumber() const {return m_channelnumber;};
      void SetChannelNumber(int channelnumber){m_channelnumber = channelnumber;}
      void SetTimeStamp(uint64_t timestamp){m_timestamp=timestamp;}
      uint64_t GetTimeStamp() const {return m_timestamp;}
//	std::string GetName() const {return m_sensor+(std::string)"_"+m_type+(std::string)to_string(m_id);}
      float getMinInRange(int min, int max) const;
      float getMaxInRange(int min, int max) const;
      float getAbsMaxInRange(int min,int max) const;
      uint16_t getIndexMin(int min, int max) const;
      uint16_t getIndexMax(int min, int max) const;
      int getIndexAbsMax(int min,int max) const;

      uint16_t getIndex(uint16_t min, uint16_t max, signed char pol) const {
        return (pol * 1 > 0) ? getIndexMax(min, max) : getIndexMin(min, max);
      }

      std::pair<int,float> getAbsMaxAndValue(int min, int max) const{
        int index = getIndexAbsMax(min,max);
        return std::make_pair(index,m_samples.at(index));
      }
      float getMedian(uint32_t min, uint32_t max) const;

      std::vector<uint16_t> * getAllPeaksAbove(uint16_t min, uint16_t max, float threshold) const;
      std::pair<uint16_t, float> getMaxPeak() const;
      float getSpreadInRange(int min, int max) const{return (getMaxInRange(min,max)-getMinInRange(min,max));};
      float getPeakToPeak(int min, int max) const{return getSpreadInRange(min,max);}
      float getIntegral(uint16_t min, uint16_t max, bool _abs=false) const;
      float getIntegral(uint16_t low_bin, uint16_t high_bin, uint16_t peak_pos, float sspeed) const;
      std::vector<float> getCalibratedTimes(std::vector<float>*) const;
      float getTriggerTime(std::vector<float>*) const;
      float getPeakFit(uint16_t, uint16_t, signed char) const;
      TF1 getRFFit(std::vector<float>*) const;
      TF1 getErfFit(uint16_t, uint16_t, signed char) const;
      float getRiseTime(uint16_t bin_low, uint16_t bin_high, float noise) const;
      float getFallTime(uint16_t bin_low, uint16_t bin_high, float noise) const;
      float getWFStartTime(uint16_t bin_low, uint16_t bin_high, float noise, float max_value) const;
      std::pair<float, float> fitMaximum(uint16_t bin_low, uint16_t bin_high) const;
      float interpolateTime(uint16_t ibin, float value) const;
      float interpolateVoltage(uint16_t ibin, float time) const;
      float getBinWidth(uint16_t ibin) const { return m_times.at(ibin) - m_times.at(uint16_t(ibin - 1)); }

  private:
      uint64_t m_timestamp;
      uint16_t m_n_samples;
      int m_channelnumber;
      mutable std::vector<float> m_samples;
      unsigned m_id;
      std::string m_type, m_sensor, m_channelname;
      uint16_t m_trigger_cell;
      signed char m_polarity;
      signed char m_pulser_polarity;
      std::vector<float> m_times;

  };

} // namespace eudaq

#endif // EUDAQ_INCLUDED_StandardWaveform
