#include "eudaq/StandardWaveform.hh"
#include <algorithm>
#include "TF1.h"
#include "TGraph.h"
#include "TGraphErrors.h"
#include <TMath.h>

using namespace std;

namespace eudaq {

eudaq::StandardWaveform::StandardWaveform(unsigned id, const std::string &type, const std::string &sensor) : m_channelnumber(-1) {
  m_id = id;
  m_type = type;
  m_sensor = sensor;
}

eudaq::StandardWaveform::StandardWaveform(Deserializer &ds) : m_samples(0), m_channelnumber(-1) {
  ds.read(m_type);
  ds.read(m_sensor);
  ds.read(m_id);
  ds.read(m_channelnumber);
}

eudaq::StandardWaveform::StandardWaveform() : m_id(0), m_sensor(""), m_type(""), m_channelnumber(-1) {}

void eudaq::StandardWaveform::Serialize(Serializer &ser) const {
  ser.write(m_type);
  ser.write(m_sensor);
  ser.write(m_id);
  ser.write(m_channelnumber);
}

void eudaq::StandardWaveform::SetNSamples(unsigned n_samples) {
  m_n_samples = n_samples;
}

void StandardWaveform::Print(std::ostream &os) const {
  os << m_id << ", " << m_type << ":" << m_sensor << ", [";
  for (size_t i = 0; i < m_samples.size() && i < m_n_samples; i++) {
    os << m_samples[i] << " ";
  }
  os << "]";
}

unsigned StandardWaveform::ID() const {
  return m_id;
}

float StandardWaveform::getMinInRange(int min, int max) const {
  return (*min_element(&m_samples.at(min), &m_samples.at(max)));
}

float StandardWaveform::getMaxInRange(int min, int max) const {
  return (*max_element(&m_samples.at(min), &m_samples.at(max)));
}

float StandardWaveform::getAbsMaxInRange(int min, int max) const {
  return abs(m_samples.at(getIndexAbsMax(min,max)));
}

float StandardWaveform::getIntegral(uint16_t min, uint16_t max, bool _abs) const {
  if (max > this->GetNSamples() - 1) max = uint16_t(this->GetNSamples() - 1);
  if (min < 0) min = 0;
  float integral = 0;
  for (uint16_t i = min; i <= max; i++) {
    if (!_abs)
      integral += m_samples.at(i);
    else
      integral += std::abs(m_samples.at(i));
  }
  return integral / (float) (max - (int) min);
}

float StandardWaveform::getIntegral(uint16_t low_bin, uint16_t high_bin, uint16_t peak_pos, float sspeed) const {

  high_bin = min(high_bin, uint16_t(m_n_samples - 1));
  if (high_bin == peak_pos and low_bin == peak_pos)
    return m_samples.at(peak_pos);
  float max_low_length = float(peak_pos - low_bin) / sspeed;
  float max_high_length = float(high_bin - peak_pos) / sspeed;
  float integral = 0;  // take the value at the peak pos as start value
  // sum up the times if the bins to the left side of the peak pos until max length is reached
  float low_length(0), high_length(0);
  uint16_t ibin(peak_pos);
  while (low_length <= max_low_length - 0.001) {
    float width = min(getBinWidth(--ibin), max_low_length - low_length);  // take the diff between max und current length if it gets smaller than bin width
    bool full_int = width < max_low_length - low_length; // full integral if rest of the total width is bigger than the actual bin width
    integral += width * (full_int ? (m_samples.at(ibin) + m_samples.at(ibin + 1)) / 2 : interpolateVoltage(ibin + 1, m_times.at(ibin + 1) - width));
    low_length += width;
  }
  ibin = uint16_t(peak_pos - 1);
  while (high_length <= max_high_length - 0.001) {
    float width = min(getBinWidth(++ibin), max_high_length - high_length);  // take the diff between max und current length if it gets smaller than bin width
    bool full_int = width < max_high_length - high_length; // full integral if rest of the total width is bigger than the actual bin width
    integral += width * (full_int ? (m_samples.at(ibin) + m_samples.at(ibin + 1)) / 2 : interpolateVoltage(ibin + 1, m_times.at(ibin) + width));
    high_length += width;
  }
  return integral / (max_high_length + max_low_length);
}

TF1 StandardWaveform::getRFFit(std::vector<float> *tcal) const {

  std::vector<float> t = getCalibratedTimes(tcal);
  auto * bla = new TGraph(5);
  auto * gr = new TGraph(unsigned(t.size()), &t[0], &m_samples[0]);
  TF1 fit("rf_fit", "[0] * TMath::Sin((x+[2])*2*pi/[1])+[3]", 0, 1000);
  fit.SetParameters(100, 20, 3, -40);
  fit.SetParLimits(0, 10, 500);
  fit.SetParLimits(2, -35, 35);
  gr->Fit("rf_fit", "q");
  return fit;
}

std::vector<float> StandardWaveform::getCalibratedTimes(std::vector<float> *tcal) const {

  std::vector<float> t = {tcal->at(m_trigger_cell)};
  for (uint16_t i(1); i < m_n_samples; i++)
    t.push_back(tcal->at(unsigned((m_trigger_cell + i) % m_n_samples)) + t.back());
  return t;
}

float StandardWaveform::getPeakFit(uint16_t bin_low, uint16_t bin_high, signed char pol) const {

  uint16_t high_bin = getIndex(bin_low, bin_high, pol);
  float t_high = m_times.at(high_bin);
  vector<float> t = vector<float>(m_times.begin() + high_bin - 50, m_times.begin() + high_bin + 5);
  std::vector<float> v = std::vector<float>(m_samples.begin() + high_bin - 50, m_samples.begin() + high_bin + 5);
  TGraph gr = TGraph(unsigned(t.size()), &t[0], &v[0]);
  TF1 fit("fit", "[0]*TMath::Landau(x, [1], [2]) - [3]", 0, 500);
  fit.SetParameters(pol * 1000, t_high, 5, pol * 5);
  gr.Fit("fit", "q");
  return float(fit.GetParameter(1));
}

TF1 StandardWaveform::getErfFit(uint16_t bin_low, uint16_t bin_high, signed char pol) const {

  TF1 fit("fit", "[0]*TMath::Erf((x-[1])*[2]) + [3]", 0, 500);
  if (getAbsMaxInRange(bin_low, bin_high) > 2000) {
    fit.SetParameters(0, 0, 0, 0);
    return fit;
  }
  uint16_t high_bin = getIndex(bin_low, bin_high, pol);
  float t_high = m_times.at(high_bin);
  TGraph gr = TGraph(unsigned(m_times.size()), &m_times[0], &m_samples[0]);
  fit.SetParameters(100, t_high, pol * .5, pol * 100);
  fit.SetParLimits(0, 10, 500);
  fit.SetParLimits(1, t_high - 20, t_high + 2);
  gr.Fit("fit", "q", "", t_high - 20, t_high + 1);
  return fit;
}

float StandardWaveform::interpolateTime(uint16_t ibin, float value) const {

  /** v = mt + a */
  float m = (m_samples.at(ibin) - m_samples.at(uint16_t(ibin - 1))) / (m_times.at(ibin) - m_times.at(uint16_t(ibin - 1)));
  float a = m_samples.at(ibin) - m * m_times.at(ibin);
  return (value - a) / m;
}

float StandardWaveform::interpolateVoltage(uint16_t ibin, float time) const {

  /** v = mt + a */
  float m = (m_samples.at(ibin) - m_samples.at(uint16_t(ibin - 1))) / (m_times.at(ibin) - m_times.at(uint16_t(ibin - 1)));
  float a = m_samples.at(ibin) - m * m_times.at(ibin);
  return m * time + a;
}

float StandardWaveform::getRiseTime(uint16_t bin_low, uint16_t bin_high, float noise) const {

  uint16_t max_index = getIndex(bin_low, bin_high, m_polarity);
  float max_value = m_samples.at(max_index) - noise;
  float t_start = m_times.at(bin_low);
  float t_stop = m_times.at(uint16_t(max_index - 1));
  bool found_stop(false);
  for (uint16_t i(max_index); i > bin_low; i--) {
    if (fabs(m_samples.at(i) - noise) < std::fabs(max_value) * .8 and not found_stop) {
      t_stop = interpolateTime(i, float(.8 * max_value));
      found_stop = true;
    }
    if (fabs(m_samples.at(i) - noise) < std::fabs(max_value) * .2) {
      t_start = interpolateTime(i, float(.2 * max_value));
      break;
    }
  }
  return t_stop - t_start;
}

float StandardWaveform::getFallTime(uint16_t bin_low, uint16_t bin_high, float noise) const {

  uint16_t max_index = getIndex(bin_low, bin_high, m_polarity);
  float t_start = m_times.at(uint16_t(max_index + 1));
  float t_stop = m_times.at(bin_high);
  float max_value = m_samples.at(max_index) - noise;
  bool found_start(false);
  for (uint16_t i(max_index); i < bin_high; i++) {
    if (fabs(m_samples.at(i) - noise) <= std::fabs(max_value) * .8 and not found_start) {
      t_start = interpolateTime(i, float(.8 * max_value));
      found_start = true;
    }
    if (fabs(m_samples.at(i) - noise) <= std::fabs(max_value) * .2) {
      t_stop = interpolateTime(i, float(.2 * max_value));
      break;
    }
  }
  return t_stop - t_start;
}

float StandardWaveform::getWFStartTime(uint16_t bin_low, uint16_t bin_high, float noise, float max_value) const {

  uint16_t max_index = getIndex(bin_low, bin_high, m_polarity);
  TGraphErrors g;
  uint16_t i_point(0);
  float max = std::fabs(max_value) - noise;
  for (uint16_t i(max_index); i > bin_low; i--)
    if (max * .2 <= fabs(m_samples.at(i) - noise) and fabs(m_samples.at(i) - noise) <= max * .8)
      g.SetPoint(i_point++, m_times.at(i), m_samples.at(i));
  TF1 fit("fit", "pol1", m_times.at(bin_low), m_times.at(bin_high));
  g.Fit(&fit, "q");
  return float(fit.GetX(noise));
}

std::pair<float, float> StandardWaveform::fitMaximum(uint16_t bin_low, uint16_t bin_high) const {

  TGraph gr;
  TF1 fit("landau", "landau + [3]");
  uint16_t max_index = getIndex(bin_low, bin_high, m_polarity);
  for (auto i(uint16_t(max_index - 6)); i <= max_index + 6; i++)
    gr.SetPoint(i - max_index + 6, m_times.at(i), m_samples.at(i));
  fit.SetParameters(m_polarity * 300, m_times.at(max_index), 2, 0);
  gr.Fit(&fit, "q");
  return make_pair(fit.GetParameter(1), fit(fit.GetParameter(1)));
}

float StandardWaveform::getTriggerTime(std::vector<float> *tcal) const {

  float min = calc_mean(std::vector<float>(m_samples.begin() + 5, m_samples.begin() + 15)).first;
  float max = calc_mean(std::vector<float>(m_samples.end() - 15, m_samples.end() - 5)).first;
  float half = (max + min) / 2;
  std::pair<float, float> p1, p2;
  for (uint16_t i(5); i < m_n_samples; i++) {
    if (m_samples.at(i) > half) {
      p1 = std::make_pair(getCalibratedTimes(tcal).at(uint16_t(i - 1)), m_samples.at(uint16_t(i - 1)));
      p2 = std::make_pair(getCalibratedTimes(tcal).at(i), m_samples.at(i));
      break;
    }
  }
  // take a straight line y = mx + a through the two points and calculate at which time it's a the half point
  float m = (p2.second - p1.second) / (p2.first - p1.first);
  float a = p1.second - m * p1.first;
  return (half - a) / m;
}

std::pair<uint16_t, float> StandardWaveform::getMaxPeak() const {
  auto max = std::max_element(m_samples.begin(), m_samples.end());
  auto min = std::min_element(m_samples.begin(), m_samples.end());
  auto peak = (abs(int(*max)) > abs(int(*min))) ? max : min;
  return std::make_pair(uint16_t(std::distance(m_samples.begin(), peak)), *peak);
}

std::vector<uint16_t> *StandardWaveform::getAllPeaksAbove(uint16_t min, uint16_t max, float threshold) const {

  auto * peak_positions = new std::vector<uint16_t>;
  uint16_t low(0), high;

  for (auto j(min + 1); j <= max - 1; j++) {
    float val = std::abs(m_samples.at(j));
    //find point when waveform gets above threshold
    if (val > threshold and std::abs(m_samples.at(uint16_t(j - 1))) < threshold)
      low = j;
    //find point when waveform gets below threshold and find max in between
    if (val > threshold and std::abs(m_samples.at(uint16_t(j + 1))) < threshold) {
      high = j;
      // the peak has to have a certain width -> avoid spikes
      if (high > low + 5) {
        auto min_el = std::min_element(m_samples.begin() + low, m_samples.begin() + high);
        auto max_el = std::max_element(m_samples.begin() + low, m_samples.begin() + high);
        auto peak = (std::abs((*max_el)) > std::abs(*min_el)) ? max_el : min_el;
        uint16_t pos = uint16_t(std::distance(m_samples.begin(), peak));
        if (peak_positions->empty())
          peak_positions->push_back(pos);
          // value has to be at least in the next bunch
        else if (pos > peak_positions->back() + 35)
          peak_positions->push_back(pos);
      }
    }
  }
  return peak_positions;
}

float StandardWaveform::getMedian(uint32_t min, uint32_t max) const {
  float median;
  uint32_t n = max >= min ? max - min + 1 : min - max + 1;
  median = (float) TMath::Median(n, &m_samples.at(min));
  return median;
}

uint16_t StandardWaveform::getIndexMin(int min, int max) const {
  float* min_el = std::min_element(&m_samples.at(min), &m_samples.at(max));
  return uint16_t(std::distance(&m_samples.at(0),min_el));//-m_samples.begin();
}

uint16_t StandardWaveform::getIndexMax(int min, int max) const {
  float* max_el = std::max_element(&m_samples.at(min), &m_samples.at(max));
  return uint16_t(std::distance(&m_samples.at(0),max_el));//-m_samples.begin();
}

int StandardWaveform::getIndexAbsMax(int min, int max) const {
  uint16_t mi = getIndexMin(min, max);
  uint16_t ma = getIndexMax(min, max);
  return abs(m_samples.at(mi)) > abs(m_samples.at(ma)) ? mi : ma;
}
}