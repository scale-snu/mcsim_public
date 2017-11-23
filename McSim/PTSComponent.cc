/*
 * Copyright (c) 2010 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Jung Ho Ahn
 */

#include "McSim.h"
#include "PTSComponent.h"
#include "PTSCore.h"

using namespace PinPthread;

extern ostream & operator << (ostream & output, coherence_state_type cs);
extern ostream & operator << (ostream & output, component_type ct);
extern ostream & operator << (ostream & output, event_type et);


void LocalQueueElement::display()
{
  cout << "  -- LQE : type = " << type << ", addr = 0x" << hex << address << dec;
  std::stack<Component *> temp;

  while (from.empty() == false)
  {
    temp.push(from.top());
    from.pop();
    cout << " (" << temp.top()->type << ", " << temp.top()->num << "), ";
  }
  while (temp.empty() == false)
  {
    from.push(temp.top());
    temp.pop();
  }
  cout << endl;
}



Component::Component(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
:type(type_), num(num_), mcsim(mcsim_), geq(mcsim_->global_q)
{
  mcsim->comps.push_back(this);
}



void Component::display()
{
  cout << "(" << type << ", " << num << ")";
}



const char * Component::prefix_str() const
{
  switch (type)
  {
    case ct_lsu:       return "pts.lsu.";
    case ct_o3core:    return "pts.o3core.";
    case ct_o3core_t1:    return "o3.t1.";
    case ct_o3core_t2:    return "o3.t2.";
    case ct_cachel1d:  return "pts.l1d$.";
    case ct_cachel1d_t1:  return "l1d$.t1.";
    case ct_cachel1d_t2:  return "l1d$.t2.";
    case ct_cachel1i:  return "pts.l1i$.";
    case ct_cachel1i_t1:  return "l1i$.t1.";
    case ct_cachel1i_t2:  return "l1i$.t2.";
    case ct_cachel2:   return "pts.l2$.";
    case ct_cachel2_t1:return "l2$.t1.";
    case ct_cachel2_t2:return "l2$.t2.";
    case ct_directory: return "pts.dir.";
    case ct_crossbar:  return "pts.xbar.";
    case ct_memory_controller: return "pts.mc.";
    case ct_tlbl1d:    return "pts.l1dtlb.";
    case ct_tlbl1i:    return "pts.l1itlb.";
    case ct_tlbl2:     return "pts.l2tlb.";
    case ct_mesh:      return "pts.mesh.";
    case ct_ring:      return "pts.ring.";
    default:           return "pts.";
  }
}



uint32_t Component::get_param_uint64(const string & param, uint32_t def) const
{
  return get_param_uint64(param, prefix_str(), def);
}



uint32_t Component::get_param_uint64(const string & param, const string & prefix, uint32_t def) const
{
  return mcsim->pts->get_param_uint64(prefix+param, def);
}



string Component::get_param_str(const string & param) const
{
  if (mcsim->pts->params.find(prefix_str()+param) != mcsim->pts->params.end())
  {
    return mcsim->pts->params.find(prefix_str()+param)->second;
  }
  else
  {
    return string();
  }
}



uint32_t Component::log2(uint64_t num)
{
  ASSERTX(num > 0);
  uint32_t log2 = 0;

  while (num > 1)
  {
    num = (num >> 1);
    log2++;
  }

  return log2;
}



GlobalEventQueue::GlobalEventQueue(McSim * mcsim_)
:event_queue(), curr_time(0), mcsim(mcsim_)
{
  num_hthreads = mcsim->pts->get_param_uint64("pts.num_hthreads", max_hthreads);
  num_mcs      = mcsim->pts->get_param_uint64("pts.num_mcs", 2);
  interleave_base_bit = mcsim->pts->get_param_uint64("pts.mc.interleave_base_bit", 12);
  interleave_xor_base_bit = mcsim->pts->get_param_uint64("pts.mc.interleave_xor_base_bit", 20);
  page_sz_base_bit = mcsim->pts->get_param_uint64("pts.mc.page_sz_base_bit", 12);
  is_asymmetric = mcsim->pts->get_param_str("is_asymmetric") == "true" ? true : false;
}



GlobalEventQueue::~GlobalEventQueue()
{
  //display();
}



void GlobalEventQueue::add_event(
    uint64_t event_time,
    Component * event_target)
{
  event_queue[event_time].insert(event_target);
}



uint32_t GlobalEventQueue::process_event()
{
  uint32_t ret_val;
  Component * p_comp;

  while (true)
  {
    event_queue_t::iterator event_queue_iter = event_queue.begin();

    if (event_queue_iter != event_queue.end())
    {
      curr_time = event_queue_iter->first;
      std::set<Component *>::iterator comp_iter = event_queue_iter->second.begin();
      if (comp_iter == event_queue_iter->second.end())
      {
        display();  ASSERTX(0);
      }

      switch ((*comp_iter)->type)
      {
        case ct_core:
        case ct_o3core:
        case ct_o3core_t1:
        case ct_o3core_t2:
          p_comp = *comp_iter;

          event_queue_iter->second.erase(comp_iter);
          if (event_queue_iter->second.empty() == true)
          {
            event_queue.erase(event_queue_iter);
          }

          ret_val = p_comp->process_event(curr_time);
          if (ret_val < num_hthreads)
          {
            return ret_val;
          }
          break;
        case ct_cachel1d:
        case ct_cachel1d_t1:
        case ct_cachel1d_t2:
        case ct_cachel1i:
        case ct_cachel1i_t1:
        case ct_cachel1i_t2:
        case ct_cachel2:
        case ct_cachel2_t1:
        case ct_cachel2_t2:
        case ct_directory:
        case ct_memory_controller:
        case ct_crossbar:
        case ct_tlbl1d:
        case ct_tlbl1i:
        case ct_mesh:
        case ct_ring:
          p_comp = *comp_iter;

          p_comp->process_event(curr_time);

          event_queue.begin()->second.erase(comp_iter);
          if (event_queue_iter->second.empty() == true)
          {
            event_queue.erase(event_queue_iter);
          }
          break;
        default:
          cout << "  -- unsupported component type " << (*comp_iter)->type << endl;
          exit(1);
          break;
      }
    }
    else
    {
      for (uint32_t i = 0; i < mcsim->cores.size(); i++)
      {
        Core * core = mcsim->cores[i];
        for (uint32_t j = 0; j < core->hthreads.size(); j++)
        {
          Hthread * hthread = core->hthreads[j];
          if (/*hthread->mem_acc.empty() == true && 
                core->is_active[j] == true &&*/
              hthread->active == true)
          {
            //core->is_active[j] = false;
            return hthread->num;
          }
        }
      }

      /*if (true)
      {
        cout << mcsim->global_q->curr_time << endl;
        for (uint32_t i = 0; i < mcsim->cores.size(); i++)
        {
          Core * core = mcsim->cores[i];
          for (uint32_t j = 0; j < core->hthreads.size(); j++)
          {
            Hthread * hthread = core->hthreads[j];
            cout << hthread->mem_acc.empty() << ", ";
            cout << core->is_active[j] << ", ";
            cout << hthread->active << ": ";
            cout << hthread->resume_time << ", " << hthread->latest_bmp_time << endl;
          }
        }

      }*/

      cout << "  -- event became empty at cycle = " << curr_time << endl;
      return num_hthreads;
      //ASSERTX(0);
    }
  }
}



void GlobalEventQueue::display()
{
  event_queue_t::iterator event_queue_iter = event_queue.begin();

  cout << "  -- global event queue : at cycle = " << curr_time << endl;

  while (event_queue_iter != event_queue.end())
  {
    cout << event_queue_iter->first << ", ";

    std::set<Component *>::iterator comp_iter = event_queue_iter->second.begin();
    while (comp_iter != event_queue_iter->second.end())
    {
      cout << "(" << (*comp_iter)->type << ", "
        << (*comp_iter)->num << "), ";
      ++comp_iter;
    }
    cout << endl;
    ++event_queue_iter;
  }
}



uint32_t GlobalEventQueue::which_mc(uint64_t address)
{
  //  return (address >> interleave_base_bit) % num_mcs;
  return ((address >> interleave_base_bit) ^ (address >> interleave_xor_base_bit)) % num_mcs;
}


