/**
 *
 *  Copyright 2016-2019 Netflix, Inc.
 *
 *     Licensed under the Apache License, Version 2.0 (the "License");
 *     you may not use this file except in compliance with the License.
 *     You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 *
 */

#include "libvmaf.h"
#include "vmaf.h"
#include <cstdio>
#include "cpu.h"

StatVector::StatVector() 
{
}

StatVector::StatVector(std::vector<double> l) : l(l) 
{
}

std::vector<double> StatVector::getVector()
{
    return l;
}

double StatVector::mean()
{
    _assert_size();
    double sum = 0.0;
    for (double e : l)
    {
        sum += e;
    }
    return sum / l.size();
}

double StatVector::minimum()
{
    _assert_size();
    double min_ = l[0];
    for (double e : l)
    {
        if (e < min_)
        {
            min_ = e;
        }
    }
    return min_;
}

double StatVector::harmonic_mean()
{
    _assert_size();
    double sum = 0.0;
    for (double e : l)
    {
        sum += 1.0 / (e + 1.0);
    }
    return 1.0 / (sum / l.size()) - 1.0;
}

double StatVector::second_moment()
{
    _assert_size();
    double sum = 0.0;
    for (double e : l)
    {
        sum += pow(e, 2);
    }
    return sum / l.size();
}

double StatVector::percentile(double perc)
{
    _assert_size();
    if (perc < 0.0) {
        perc = 0.0;
    }
    else if (perc > 100.0) {
        perc = 100.0;
    }
    std::vector<double> l(this->l);
    std::sort(l.begin(), l.end());
    double pos = perc * (this->l.size() - 1) / 100.0;
    int pos_left = (int)floor(pos);
    int pos_right = (int)ceil(pos);
    if (pos_left == pos_right) {
        return l[pos_left];
    }
    else {
        return l[pos_left] * (pos_right - pos) + l[pos_right] * (pos - pos_left);
    }

}

double StatVector::var()
{ 
    return second_moment() - pow(mean(), 2); 
}

double StatVector::std()
{ 
    return sqrt(var()); 
}

void StatVector::append(double e)
{ 
    l.push_back(e); 
}

double StatVector::at(size_t idx)
{ 
    return l.at(idx); 
}

size_t StatVector::size()
{ 
    return l.size(); 
}

void StatVector::_assert_size()
{
    if (l.size() == 0) {
        throw std::runtime_error("StatVector size is 0.");
    }
}

Result::Result() : score_aggregate_method(ScoreAggregateMethod::MEAN)
{
}

void Result::set_scores(const std::string &key, const StatVector &scores)
{
    d[key] = scores; 
}

StatVector Result::get_scores(const std::string &key)
{ 
    return d[key]; 
}

bool Result::has_scores(const std::string &key)
{
    return d.find(key) != d.end(); 
}

double Result::get_score(const std::string &key)
{
    StatVector list = get_scores(key);
    if (score_aggregate_method == ScoreAggregateMethod::MINIMUM)
    {
        return list.minimum();
    }
    else if (score_aggregate_method == ScoreAggregateMethod::HARMONIC_MEAN)
    {
        return list.harmonic_mean();
    }
    else // MEAN
    {
        return list.mean();
    }
}

std::vector<std::string> Result::get_keys()
{
    std::vector<std::string> v;
    for (std::map<std::string, StatVector>::iterator it = d.begin(); it != d.end(); ++it)
    {
        v.push_back(it->first);
    }
    return v;
}

unsigned int Result::get_num_frms()
{
    return num_frms;
}

void Result::set_num_frms(unsigned int num_frms)
{
    this->num_frms = num_frms;
}

void Result::setScoreAggregateMethod(ScoreAggregateMethod scoreAggregateMethod)
{
    score_aggregate_method = scoreAggregateMethod;
}

std::unique_ptr<IVmafQualityRunner> 
VmafQualityRunnerFactory::createVmafQualityRunner(VmafModel *vmaf_model_ptr) {
    std::unique_ptr<IVmafQualityRunner> runner_ptr;
    if (vmaf_model_ptr->enable_conf_interval)
    {
        runner_ptr = std::unique_ptr<BootstrapVmafQualityRunner>(new BootstrapVmafQualityRunner(vmaf_model_ptr->path));
    }
    else
    {
        runner_ptr = std::unique_ptr<VmafQualityRunner>(new VmafQualityRunner(vmaf_model_ptr->path));
    }
    return runner_ptr;
}

extern "C" {

    enum vmaf_cpu cpu; // global

    int compute_vmaf(double* vmaf_score,
        int(*read_frame)(float *ref_data, float *main_data, float *temp_data, int stride_byte, void *user_data),
        int(*read_vmaf_picture)(VmafPicture *ref_vmaf_pict, VmafPicture *dis_vmaf_pict, float *temp_data, int stride, void *user_data),
        void *user_data, VmafSettings *vmafSettings)
    {

        cpu = cpu_autodetect();

        if (vmafSettings->disable_avx)
        {
            cpu = VMAF_CPU_NONE;
        }

        try {
            double score = RunVmaf(read_frame, read_vmaf_picture, user_data, vmafSettings);
            *vmaf_score = score;
            return 0;
        }
        catch (VmafException& e)
        {
            printf("Caught VmafException: %s\n", e.what());
            return -2;
        }
        catch (std::runtime_error& e)
        {
            printf("Caught runtime_error: %s\n", e.what());
            return -3;
        }
        catch (std::logic_error& e)
        {
            printf("Caught logic_error: %s\n", e.what());
            return -4;
        }
    }
}
