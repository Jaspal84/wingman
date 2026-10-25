#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <optional>
namespace wingman {
class TapTempo {
    std::vector<double> taps;
public:
    std::optional<double> tap(double seconds){
        if(!std::isfinite(seconds))return {};
        if(!taps.empty()&&(seconds<=taps.back()||seconds-taps.back()>2))taps.clear();
        taps.push_back(seconds);if(taps.size()>9)taps.erase(taps.begin());if(taps.size()<2)return {};
        const double interval=(taps.back()-taps.front())/(taps.size()-1);
        if(interval<0.25||interval>1.5)return {};return 60/interval;
    }
};
struct TempoResult {bool valid=false;double bpm=0,confidence=0,halfTime=0,doubleTime=0;std::vector<double> onsets,beats;};
inline TempoResult analyseTempo(const float* samples,int count,double rate,const std::function<bool(double)>& next={},const float* right=nullptr){
    TempoResult result;if(!samples||!std::isfinite(rate)||rate<=0||count<rate*3)return result;
    const int hop=std::max(1,static_cast<int>(rate/200));const double envelopeRate=rate/hop;
    std::vector<double> onset;double previous=0,maximum=0;
    for(int i=0;i+hop<=count;i+=hop){
        if(next&&i%(hop*64)==0&&!next(i/static_cast<double>(count)*0.5))throw std::runtime_error("Tempo analysis cancelled");
        double sum=0;for(int j=0;j<hop;++j){if(!std::isfinite(samples[i+j]))throw std::invalid_argument("Nonfinite tempo input");if(right&&!std::isfinite(right[i+j]))throw std::invalid_argument("Nonfinite tempo input");sum+=right?(samples[i+j]*static_cast<double>(samples[i+j])+right[i+j]*static_cast<double>(right[i+j]))*0.5:samples[i+j]*static_cast<double>(samples[i+j]);}
        const double energy=std::sqrt(sum/hop),rise=std::max(0.0,energy-previous);previous=energy;onset.push_back(rise);maximum=std::max(maximum,rise);
    }
    if(maximum<1e-6)return result;
    const int refractory=std::max(1,static_cast<int>(envelopeRate*0.15));int last=-refractory;
    for(int i=1;i+1<static_cast<int>(onset.size());++i){
        if(onset[static_cast<std::size_t>(i)]>=maximum*0.28&&onset[static_cast<std::size_t>(i)]>=onset[static_cast<std::size_t>(i-1)]&&onset[static_cast<std::size_t>(i)]>onset[static_cast<std::size_t>(i+1)]&&i-last>=refractory){result.onsets.push_back(i/envelopeRate);last=i;}
    }
    if(result.onsets.size()<4)return result;
    std::vector<double> intervals;for(std::size_t i=1;i<result.onsets.size();++i){auto interval=result.onsets[i]-result.onsets[i-1];if(interval>=0.24&&interval<=1.5)intervals.push_back(interval);}
    if(intervals.size()<3)return result;
    std::sort(intervals.begin(),intervals.end());const double period=intervals[intervals.size()/2];
    double error=0;for(auto interval:intervals)error+=std::abs(interval-period)/period;
    result.confidence=std::clamp(1.0-error/intervals.size()*3,0.0,1.0);
    if(result.confidence<0.35)return result;
    // Regression over consecutive detected pulses removes hop quantization jitter.
    double xy=0,xx=0;const double mean=(result.onsets.size()-1)*0.5;
    for(std::size_t i=0;i<result.onsets.size();++i){xy+=(i-mean)*result.onsets[i];xx+=(i-mean)*(i-mean);}
    const double fitted=xx>0?xy/xx:period;
    const double chosen=std::abs(fitted-period)<period*0.1?fitted:period;
    result.bpm=60/chosen;result.halfTime=result.bpm/2;result.doubleTime=result.bpm*2;result.valid=result.bpm>=40&&result.bpm<=240;
    if(result.valid)for(double beat=result.onsets.front();beat<count/rate;beat+=chosen)result.beats.push_back(beat);
    if(next&&!next(1))throw std::runtime_error("Tempo analysis cancelled");
    return result;
}
}
