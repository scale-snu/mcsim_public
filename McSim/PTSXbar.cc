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

#include <sstream>

#include "PTSXbar.h"
#include "PTSCache.h"
#include "PTSDirectory.h"
#include <iomanip>

using namespace PinPthread;


NoC::NoC(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :Component(type_, num_, mcsim_), directory(), cachel2(),
  num_req(0), num_rep(0), num_crq(0), num_flits(0), num_data_transfers(0)
{
}



NoC::~NoC()
{
  if (num_req > 0)
  {
    std::cout << "  -- NoC [" << setw(3) << num << "] : (req, crq, rep) = ("
              << num_req << ", " << num_crq << ", " << num_rep
              << "), num_data_transfers = " << num_data_transfers << std::endl;
  }
}



Crossbar::Crossbar(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_,
    uint32_t num_ports_)
 :NoC(type_, num_, mcsim_),
  queues(num_ports_), already_sent(num_ports_),
  xbar_to_dir_t(get_param_uint64("to_dir_t", 90)),
  xbar_to_l2_t (get_param_uint64("to_l2_t", 90)),
  num_ports(num_ports_),
  clockwise(true), top_priority(0)
{
  process_interval = get_param_uint64("process_interval", 10);
}



Crossbar::~Crossbar()
{
}



void Crossbar::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  ASSERTX(from);
  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  num_req++;
  num_flits++;
  geq->add_event(event_time, this);
  req_events.insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
}



void Crossbar::add_crq_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  ASSERTX(from);
  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  num_crq++;
  num_flits++;
  geq->add_event(event_time, this);
  crq_events.insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
}



void Crossbar::add_crq_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    uint32_t num_flits,
    Component * from)
{
  for (uint32_t i = 0; i < num_flits - 1; i++)
  {
    LocalQueueElement * dummy_lqe = new LocalQueueElement();
    dummy_lqe->dummy = true;
    dummy_lqe->from  = local_event->from;
    dummy_lqe->type  = local_event->type;
    dummy_lqe->address = local_event->address;
    dummy_lqe->th_id   = local_event->th_id;
    add_crq_event(event_time, dummy_lqe, from);
    ++num_crq;
  }

  add_crq_event(event_time, local_event, from);
  num_data_transfers++;
}



void Crossbar::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  ASSERTX(from);
  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  num_rep++;
  num_flits++;
  geq->add_event(event_time, this);
  rep_events.insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
}



void Crossbar::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    uint32_t num_flits,
    Component * from)
{
  for (uint32_t i = 0; i < num_flits - 1; i++)
  {
    LocalQueueElement * dummy_lqe = new LocalQueueElement();
    dummy_lqe->dummy = true;
    dummy_lqe->from  = local_event->from;
    dummy_lqe->type  = local_event->type;
    dummy_lqe->address = local_event->address;
    dummy_lqe->th_id   = local_event->th_id;
    add_rep_event(event_time, dummy_lqe, from);
    ++num_rep;
  }

  add_rep_event(event_time, local_event, from);
  num_data_transfers++;
}



uint32_t Crossbar::process_event(uint64_t curr_time)
{
  std::multimap<uint64_t, EventPair>::iterator req_event_iter = req_events.begin();
  std::multimap<uint64_t, EventPair>::iterator rep_event_iter = rep_events.begin();
  std::multimap<uint64_t, EventPair>::iterator crq_event_iter = crq_events.begin();

  while (rep_event_iter != rep_events.end() && rep_event_iter->first == curr_time)
  {
    if (rep_event_iter->second.first->type == et_evict ||
        rep_event_iter->second.first->type == et_invalidate ||
        rep_event_iter->second.first->type == et_invalidate_nd ||
        rep_event_iter->second.first->type == et_nop ||
        rep_event_iter->second.first->type == et_e_to_i ||
        rep_event_iter->second.first->type == et_e_to_m)
    {
      uint32_t which_mc = geq->which_mc(rep_event_iter->second.first->address);
      queues[rep_event_iter->second.second->num].insert(std::pair<noc_priority, EventPair>(noc_rep, EventPair(rep_event_iter->second.first, directory[which_mc])));
    }
    else
    {
      queues[rep_event_iter->second.second->num].insert(std::pair<noc_priority, EventPair>(noc_rep, EventPair(rep_event_iter->second.first, rep_event_iter->second.first->from.top())));
    }
    ++rep_event_iter;
  }

  while (crq_event_iter != crq_events.end() && crq_event_iter->first == curr_time)
  {
    // special case --  from.top() is the target L2
    queues[crq_event_iter->second.second->num].insert(std::pair<noc_priority, EventPair>(noc_crq, EventPair(crq_event_iter->second.first, crq_event_iter->second.first->from.top())));
    crq_event_iter->second.first->from.pop();
    ++crq_event_iter;
  }

  while (req_event_iter != req_events.end() && req_event_iter->first == curr_time)
  {
    // process the first request event
    uint32_t which_mc  = geq->which_mc(req_event_iter->second.first->address);  // TODO : it is assumed that (directory[i]->num == i)
    queues[req_event_iter->second.second->num].insert(std::pair<noc_priority, EventPair>(noc_req, EventPair(req_event_iter->second.first, directory[which_mc])));
    ++req_event_iter;
  }
  
  req_events.erase(curr_time);  // add_req_event
  crq_events.erase(curr_time);  // add_rep_event
  rep_events.erase(curr_time);  // add_rep_event


  // send events to destinations
  for (uint32_t i = 0; i < num_ports; i++)
  {
    already_sent[i] = false;
  }

  for (uint32_t i = 0; i < num_ports; i++)
  {
    uint32_t idx = top_priority + num_ports;
    idx += ((clockwise == true) ? i : (0-i));
    idx %= num_ports;

    std::multimap<noc_priority, EventPair>::iterator iter = queues[idx].begin();
    if (iter != queues[idx].end())
    {
      if (already_sent[iter->second.second->num] == false)
      {
        already_sent[iter->second.second->num] = true;
        if (iter->second.first->dummy == true)
        {
          delete iter->second.first;
        }
        else if (iter->first == noc_req)
        {
          iter->second.second->add_req_event(curr_time + xbar_to_dir_t, iter->second.first);
        }
        else
        {
          iter->second.second->add_rep_event(curr_time + xbar_to_dir_t, iter->second.first);
        }
        queues[idx].erase(iter);
        iter = queues[idx].begin();
      }
      else
      {
        ++iter;
      }

      if (iter != queues[idx].end())
      {
        if (already_sent[iter->second.second->num] == false)
        {
          already_sent[iter->second.second->num] = true;
          if (iter->second.first->dummy == true)
          {
            delete iter->second.first;
          }
          else if (iter->first == noc_req)
          {
            iter->second.second->add_req_event(curr_time + xbar_to_dir_t, iter->second.first);
          }
          else
          {
            iter->second.second->add_rep_event(curr_time + xbar_to_dir_t, iter->second.first);
          }
          queues[idx].erase(iter);
        }
      }
    }
  }
  if (clockwise == true)
  {
    clockwise = false;
  }
  else
  {
    clockwise = true;
    top_priority = (top_priority + 1)%num_ports;
  }

  for (uint32_t i = 0; i < num_ports; i++)
  {
    if (queues[i].empty() == false)
    {
      geq->add_event(curr_time + process_interval, this);
      break;
    }
  }

  return 0;
}



Mesh2D::Mesh2D(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :NoC(type_, num_, mcsim_),
  mc_pos(),
  num_req_in_mesh(0),
  sw_to_sw_t(get_param_uint64("sw_to_sw_t", 10)),
  num_rows  (get_param_uint64("num_rows", 4)),
  num_cols  (get_param_uint64("num_cols", 2)),
  req_qs(num_rows, vector< vector< multimap<uint64_t, EventPair> > >(num_cols, vector< multimap<uint64_t, EventPair> >(mesh_invalid))),
  crq_qs(num_rows, vector< vector< multimap<uint64_t, EventPair> > >(num_cols, vector< multimap<uint64_t, EventPair> >(mesh_invalid))),
  rep_qs(num_rows, vector< vector< multimap<uint64_t, EventPair> > >(num_cols, vector< multimap<uint64_t, EventPair> >(mesh_invalid))),
  already_sent(mesh_invalid),
  token(0), num_hops(0), num_hops2(0)
{
  process_interval = get_param_uint64("process_interval", 10);
  uint32_t num_mcs = get_param_uint64("num_mcs", "pts.", 2);

  // specify the positions of memory controllers in a mesh network.
  for (uint32_t i = 0; i < num_mcs; i++)
  {
    stringstream out;
    out << i;
    string pos = get_param_str(string("mc_pos")+out.str());

    uint32_t row, col;

    if (pos.length() < 1 || pos.find(",", 1) == string::npos)
    {
      row = 0;
      col = 0;
    }
    else
    {
      istringstream srow(pos.substr(0, pos.find(",", 1)));
      istringstream scol(pos.substr(pos.find(",", 1) + 1));
      
      srow >> row;
      scol >> col;
    }

    if (row >= num_rows || col >= num_cols)
    {
      geq->display();  ASSERTX(0);
    }

    mc_pos.push_back(row * num_cols + col);
  }
}



Mesh2D::~Mesh2D()
{
  if (num_hops > 0)
  {
    cout << "  -- MESH[" << num << "] : average hop = "
         << 1.0 * num_hops / num_flits << endl;
  }
}



void Mesh2D::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  ASSERTX(from);
  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);
  uint32_t cluster_num = from->num;
  uint32_t col = cluster_num % num_cols;
  uint32_t row = cluster_num / num_cols;

  req_qs[row][col][mesh_cluster].insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
  ++num_req_in_mesh;
  ++num_req;
  num_flits++;
}



void Mesh2D::add_crq_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  ASSERTX(from);

  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);
  // special case --  from.top() is the target L2
  uint32_t mc_num = from->num;
  uint32_t col = mc_pos[mc_num] % num_cols;
  uint32_t row = mc_pos[mc_num] / num_cols;

  crq_qs[row][col][mesh_directory].insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
  ++num_req_in_mesh;
  ++num_crq;
  num_flits++;
}



void Mesh2D::add_crq_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    uint32_t num_flits,
    Component * from)
{
  for (uint32_t i = 0; i < num_flits - 1; i++)
  {
    LocalQueueElement * dummy_lqe = new LocalQueueElement();
    dummy_lqe->dummy = true;
    dummy_lqe->from  = local_event->from;
    dummy_lqe->type  = local_event->type;
    dummy_lqe->address = local_event->address;
    dummy_lqe->th_id   = local_event->th_id;
    add_crq_event(event_time, dummy_lqe, from);
    ++num_crq;
  }

  add_crq_event(event_time, local_event, from);
  num_data_transfers++;
}



void Mesh2D::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  ASSERTX(from);

  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);

  uint32_t col, row;
  if (local_event->type == et_evict ||
      local_event->type == et_invalidate ||
      from->type == ct_cachel2 ||
      from->type == ct_cachel2_t1 ||
      from->type == ct_cachel2_t2 ||
      local_event->type == et_invalidate_nd ||
      local_event->type == et_nop ||
      local_event->type == et_e_to_i ||
      local_event->type == et_e_to_m)
  {
    uint32_t cluster_num = from->num;
    col = cluster_num % num_cols;
    row = cluster_num / num_cols;

    rep_qs[row][col][mesh_cluster].insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
  }
  else
  {
    uint32_t mc_num = from->num;
    col = mc_pos[mc_num] % num_cols;
    row = mc_pos[mc_num] / num_cols;

    rep_qs[row][col][mesh_directory].insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
  }
  ++num_req_in_mesh;
  ++num_rep;
  num_flits++;
}



void Mesh2D::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    uint32_t num_flits,
    Component * from)
{
  for (uint32_t i = 0; i < num_flits - 1; i++)
  {
    LocalQueueElement * dummy_lqe = new LocalQueueElement();
    dummy_lqe->dummy = true;
    dummy_lqe->from  = local_event->from;
    dummy_lqe->type  = local_event->type;
    dummy_lqe->address = local_event->address;
    dummy_lqe->th_id   = local_event->th_id;
    add_rep_event(event_time, dummy_lqe, from);
    ++num_rep;
  }

  add_rep_event(event_time, local_event, from);
  num_data_transfers++;
}



uint32_t Mesh2D::process_event(
    uint64_t curr_time)
{
  // route LQEs in queues
  for (uint32_t i = 0; i < num_rows; i++)
  {
    for (uint32_t j = 0; j < num_cols; j++)
    {
      for (uint32_t k = 0; k < mesh_invalid; k++)
      {
        already_sent[k] = false;
      }

      for (uint32_t k = 0; k < mesh_invalid; k++)
      {
        uint32_t dir = (k + token) % mesh_invalid;
        process_qs(noc_rep, i, j, dir, curr_time);
      }

      for (uint32_t k = 0; k < mesh_invalid; k++)
      {
        uint32_t dir = (k + token) % mesh_invalid;
        process_qs(noc_crq, i, j, dir, curr_time);
      }

      for (uint32_t k = 0; k < mesh_invalid; k++)
      {
        uint32_t dir = (k + token) % mesh_invalid;
        process_qs(noc_req, i, j, dir, curr_time);
      }
    }
  }

  if (num_req_in_mesh > 0)
  {
    geq->add_event(curr_time + process_interval, this);
  }
  token = (token + 1) % mesh_invalid;
  return 0;
}



void Mesh2D::process_qs(
    noc_priority queue_type,
    uint32_t i,
    uint32_t j,
    uint32_t dir,
    uint64_t curr_time)
{
  vector< vector< vector< multimap<uint64_t, EventPair > > > > * curr_qs;
  Directory * to_dir = NULL;
  CacheL2   * to_l2  = NULL;
  uint32_t  col, row;

  switch (queue_type)
  {
    case noc_rep:
      curr_qs = & rep_qs;
      break;
    case noc_crq:
      curr_qs = & crq_qs;
      break;
    case noc_req:
      curr_qs = & req_qs;
      break;
  }
  
  if ((*curr_qs)[i][j][dir].empty() == true ||
      (*curr_qs)[i][j][dir].begin()->first > curr_time)
  {
    return;
  }
  EventPair curr_q = (*curr_qs)[i][j][dir].begin()->second;
  switch (queue_type)
  {
    case noc_rep:
      if (curr_q.first->type == et_evict ||
          curr_q.first->type == et_invalidate ||
          curr_q.second->type == ct_cachel2 ||
          curr_q.second->type == ct_cachel2_t1 ||
          curr_q.second->type == ct_cachel2_t2 ||
          curr_q.first->type == et_invalidate_nd ||
          curr_q.first->type == et_nop ||
          curr_q.first->type == et_e_to_m ||
          curr_q.first->type == et_e_to_i)
      {
        // to directory
        uint32_t which_mc = geq->which_mc(curr_q.first->address);
        col = mc_pos[which_mc] % num_cols;
        row = mc_pos[which_mc] / num_cols;
        to_dir = directory[which_mc];
      }
      else
      {
        uint32_t cluster_num = curr_q.first->from.top()->num;
        col = cluster_num % num_cols;
        row = cluster_num / num_cols;
        to_l2 = cachel2[cluster_num];
      }
      break;
    case noc_crq:
      {
      uint32_t cluster_num = curr_q.first->from.top()->num;
      col = cluster_num % num_cols;
      row = cluster_num / num_cols;
      to_l2 = cachel2[cluster_num];
      }
      break;
    case noc_req:
      {
      uint32_t which_mc = geq->which_mc(curr_q.first->address);
      col = mc_pos[which_mc] % num_cols;
      row = mc_pos[which_mc] / num_cols;
      to_dir = directory[which_mc];
      }
      break;
  }


  // XY dimension order routing
  if (j > col)
  {
    if (already_sent[mesh_west] == true)
    {
      return;
    }
    already_sent[mesh_west] = true;
    (*curr_qs)[i][j-1][mesh_east].insert(pair<uint64_t, EventPair>(curr_time + sw_to_sw_t, curr_q));
  }
  else if (j < col)
  {
    if (already_sent[mesh_east] == true)
    {
      return;
    }
    already_sent[mesh_east] = true;
    (*curr_qs)[i][j+1][mesh_west].insert(pair<uint64_t, EventPair>(curr_time + sw_to_sw_t, curr_q));
  }
  else if (i > row)
  {
    if (already_sent[mesh_north] == true)
    {
      return;
    }
    already_sent[mesh_north] = true;
    (*curr_qs)[i-1][j][mesh_south].insert(pair<uint64_t, EventPair>(curr_time + sw_to_sw_t, curr_q));
  }
  else if (i < row)
  {
    if (already_sent[mesh_south] == true)
    {
      return;
    }
    already_sent[mesh_south] = true;
    (*curr_qs)[i+1][j][mesh_north].insert(pair<uint64_t, EventPair>(curr_time + sw_to_sw_t, curr_q));
  }
  else
  {
    if (to_dir != NULL)
    {
      // to directory
      if (already_sent[mesh_directory] == true)
      {
        return;
      }
      if (queue_type == noc_crq)
      {
        curr_q.first->from.pop();
      }
      already_sent[mesh_directory] = true;
      //curr_q.first->from.push(this);
      if (curr_q.first->dummy == true)
      {
        delete curr_q.first;
      }
      else if (queue_type == noc_req)
      {
        to_dir->add_req_event(curr_time + sw_to_sw_t, curr_q.first);
        //cout << to_dir->num << ", " << curr_time << endl;
      }
      else
      {
        to_dir->add_rep_event(curr_time + sw_to_sw_t, curr_q.first);
      }
      num_req_in_mesh--;
    }
    else
    {
      if (already_sent[mesh_cluster] == true)
      {
        return;
      }
      already_sent[mesh_cluster] = true;
      if (queue_type == noc_crq)
      {
        curr_q.first->from.pop();
      }

      if (curr_q.first->dummy == true)
      {
        delete curr_q.first;
      }
      else
      {
        //curr_q.first->from.push(this);
        to_l2->add_rep_event(curr_time + sw_to_sw_t, curr_q.first);
      }

      num_req_in_mesh--;
    }
  }
  (*curr_qs)[i][j][dir].erase((*curr_qs)[i][j][dir].begin());
  num_hops++;
}



Ring::Ring(
    component_type type_,
    uint32_t num_,
    McSim * mcsim_)
 :NoC(type_, num_, mcsim_),
  l2_pos(), mc_pos(),
  num_req_in_ring(0),
  sw_to_sw_t(get_param_uint64("sw_to_sw_t", 10)),
  num_nodes (get_param_uint64("num_nodes",  4)),
  token(0), num_hops(0)
{
  process_interval = get_param_uint64("process_interval", 10);

  uint32_t num_hthreads               = mcsim->pts->get_num_hthreads();
  uint32_t num_threads_per_l1_cache   = get_param_uint64("num_hthreads_per_l1$", "pts.", 4);
  uint32_t num_l1_caches_per_l2_cache = get_param_uint64("num_l1$_per_l2$", "pts.", 2);
  uint32_t num_mcs = get_param_uint64("num_mcs", "pts.", 2);
  uint32_t num_l2s = num_hthreads / num_threads_per_l1_cache / num_l1_caches_per_l2_cache;

  vector<uint32_t> router_radix(num_nodes, 2);

  // specify the positions of memory controllers in a ring network
  for (uint32_t i = 0; i < num_mcs; i++)
  {
    stringstream out;
    out << i;
    uint32_t pos = get_param_uint64(string("mc_pos")+out.str(), 0);

    if (pos >= num_nodes)
    {
      cout << "there are only " << num_nodes << " rings." << endl;
      ASSERTX(0);
    }

    mc_pos.push_back(pos);
    mc_port_num.push_back(router_radix[pos]);
    router_radix[pos]++;
  }
      
  // specify the positions of L2 caches in a ring network
  for (uint32_t i = 0; i < num_l2s; i++)
  {
    stringstream out;
    out << i;
    uint32_t pos = get_param_uint64(string("l2_pos")+out.str(), 0);

    if (pos >= num_nodes)
    {
      cout << "there are only " << num_nodes << " rings." << endl;
      ASSERTX(0);
    }

    l2_pos.push_back(pos);
    l2_port_num.push_back(router_radix[pos]);
    router_radix[pos]++;
  }


  // specify the maximum radix 
  uint32_t max_radix = 0;
  for (uint32_t i = 0; i < num_nodes; i++)
  {
    if (router_radix[i] > max_radix)
    {
      max_radix = router_radix[i];
    }
  }

  already_sent = vector<bool>(max_radix, false);
  req_qs = vector< vector< multimap<uint64_t, EventPair> > >(num_nodes, vector< multimap<uint64_t, EventPair> >(max_radix));
  crq_qs = vector< vector< multimap<uint64_t, EventPair> > >(num_nodes, vector< multimap<uint64_t, EventPair> >(max_radix));
  rep_qs = vector< vector< multimap<uint64_t, EventPair> > >(num_nodes, vector< multimap<uint64_t, EventPair> >(max_radix));
}



Ring::~Ring()
{
  if (num_hops > 0)
  {
    cout << "  -- RING[" << num << "] : average hop = "
         << 1.0 * num_hops / (num_req + num_crq + num_rep) << endl;
  }
}



void Ring::add_req_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  ASSERTX(from);

  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);
  uint32_t cluster_num      = from->num;
  uint32_t cluster_pos      = l2_pos[cluster_num];
  uint32_t cluster_port_num = l2_port_num[cluster_num];

  req_qs[cluster_pos][cluster_port_num].insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
  ++num_req_in_ring;
  ++num_req;
  num_flits++;
}



void Ring::add_crq_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  ASSERTX(from);

  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);
  // special case --  from.top() is the target L2
  uint32_t dir_num = from->num;
  uint32_t dir_pos = mc_pos[dir_num];
  uint32_t dir_port_num = mc_port_num[dir_num];

  crq_qs[dir_pos][dir_port_num].insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
  ++num_req_in_ring;
  ++num_crq;
  num_flits++;
}



void Ring::add_crq_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    uint32_t num_flits,
    Component * from)
{
  for (uint32_t i = 0; i < num_flits - 1; i++)
  {
    LocalQueueElement * dummy_lqe = new LocalQueueElement();
    dummy_lqe->dummy = true;
    dummy_lqe->from  = local_event->from;
    dummy_lqe->type  = local_event->type;
    dummy_lqe->address = local_event->address;
    dummy_lqe->th_id   = local_event->th_id;
    add_crq_event(event_time, dummy_lqe, from);
    ++num_crq;
  }

  add_crq_event(event_time, local_event, from);
  num_data_transfers++;
}



void Ring::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    Component * from)
{
  ASSERTX(from);

  if (event_time % process_interval != 0)
  {
    event_time = event_time + process_interval - event_time%process_interval;
  }
  geq->add_event(event_time, this);

  if (local_event->type == et_evict ||
      local_event->type == et_invalidate ||
      local_event->type == et_invalidate_nd ||
      from->type == ct_cachel2 ||
      from->type == ct_cachel2_t1 ||
      from->type == ct_cachel2_t2 ||
      local_event->type == et_nop ||
      local_event->type == et_e_to_m ||
      local_event->type == et_e_to_i)
  {
    uint32_t cluster_num      = from->num;
    uint32_t cluster_pos      = l2_pos[cluster_num];
    uint32_t cluster_port_num = l2_port_num[cluster_num];

    rep_qs[cluster_pos][cluster_port_num].insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
  }
  else
  {
    uint32_t dir_num = from->num;
    uint32_t dir_pos = mc_pos[dir_num];
    uint32_t dir_port_num = mc_port_num[dir_num];

    rep_qs[dir_pos][dir_port_num].insert(pair<uint64_t, EventPair>(event_time, EventPair(local_event, from)));
  }
  ++num_req_in_ring;
  ++num_rep;
  num_flits++;
}



void Ring::add_rep_event(
    uint64_t event_time,
    LocalQueueElement * local_event,
    uint32_t num_flits,
    Component * from)
{
  for (uint32_t i = 0; i < num_flits - 1; i++)
  {
    LocalQueueElement * dummy_lqe = new LocalQueueElement();
    dummy_lqe->dummy = true;
    dummy_lqe->from  = local_event->from;
    dummy_lqe->type  = local_event->type;
    dummy_lqe->address = local_event->address;
    dummy_lqe->th_id   = local_event->th_id;
    add_rep_event(event_time, dummy_lqe, from);
    ++num_rep;
  }

  add_rep_event(event_time, local_event, from);
  num_data_transfers++;
}



uint32_t Ring::process_event(uint64_t curr_time)
{
  // route LQEs in queues
  for (uint32_t i = 0; i < num_nodes; i++)
  {
    for (uint32_t k = 0; k < already_sent.size(); k++)
    {
      already_sent[k] = false;
    }

    for (uint32_t k = 0; k < already_sent.size(); k++)
    {
      uint32_t dir = (k + token) % already_sent.size();
      process_qs(noc_rep, i, dir, curr_time);
    }

    for (uint32_t k = 0; k < already_sent.size(); k++)
    {
      uint32_t dir = (k + token) % already_sent.size();
      process_qs(noc_crq, i, dir, curr_time);
    }

    for (uint32_t k = 0; k < already_sent.size(); k++)
    {
      uint32_t dir = (k + token) % already_sent.size();
      process_qs(noc_req, i, dir, curr_time);
    }
  }

  if (num_req_in_ring > 0)
  {
    geq->add_event(curr_time + process_interval, this);
  }
  token = (token + 1) % already_sent.size();
  return 0;
}



void Ring::process_qs(
    noc_priority queue_type,
    uint32_t i,
    uint32_t dir,
    uint64_t curr_time)
{
  vector< vector< multimap<uint64_t, EventPair > > > * curr_qs;
  Directory * to_dir = NULL;
  CacheL2   * to_l2  = NULL;
  uint32_t  target_pos;
  uint32_t  target_port_num;

  switch (queue_type)
  {
    case noc_rep:
      curr_qs = & rep_qs;
      break;
    case noc_crq:
      curr_qs = & crq_qs;
      break;
    case noc_req:
      curr_qs = & req_qs;
      break;
  }
  
  if ((*curr_qs)[i][dir].empty() == true ||
      (*curr_qs)[i][dir].begin()->first > curr_time)
  {
    return;
  }
  EventPair curr_q = (*curr_qs)[i][dir].begin()->second;
  switch (queue_type)
  {
    case noc_rep:
      if (curr_q.first->type == et_evict ||
          curr_q.first->type == et_invalidate ||
          curr_q.second->type == ct_cachel2 ||
          curr_q.second->type == ct_cachel2_t1 ||
          curr_q.second->type == ct_cachel2_t2 ||
          curr_q.first->type == et_invalidate_nd || 
          curr_q.first->type == et_nop ||
          curr_q.first->type == et_e_to_m ||
          curr_q.first->type == et_e_to_i)
      {
        // to directory
        uint32_t which_mc = geq->which_mc(curr_q.first->address);
        target_pos = mc_pos[which_mc];
        target_port_num = mc_port_num[which_mc];
        to_dir = directory[which_mc];
      }
      else
      {
        uint32_t cluster_num = curr_q.first->from.top()->num;
        target_pos = l2_pos[cluster_num];
        target_port_num = l2_port_num[cluster_num];
        to_l2 = cachel2[cluster_num];
      }
      break;
    case noc_crq:
      {
        uint32_t cluster_num = curr_q.first->from.top()->num;
        target_pos = l2_pos[cluster_num];
        target_port_num = l2_port_num[cluster_num];
        to_l2 = cachel2[cluster_num];
      }
      break;
    case noc_req:
      {
        uint32_t which_mc = geq->which_mc(curr_q.first->address);
        target_pos = mc_pos[which_mc];
        target_port_num = mc_port_num[which_mc];
        to_dir = directory[which_mc];
      }
      break;
  }


  // XY dimension order routing
  if ((target_pos > i && target_pos - i <= num_nodes/2) ||
      (target_pos < i && (num_nodes + target_pos - i) <= num_nodes/2))
  {
    // clockwise
    if (already_sent[ring_cw] == true)
    {
      return;
    }
    already_sent[ring_cw] = true;
    uint32_t router_pos = (i + 1) % num_nodes;
    (*curr_qs)[router_pos][ring_ccw].insert(pair<uint64_t, EventPair>(curr_time + sw_to_sw_t, curr_q));
  }
  else if (target_pos != i)
  {
    // counter-clockwise
    if (already_sent[ring_ccw] == true)
    {
      return;
    }
    already_sent[ring_ccw] = true;
    uint32_t router_pos = (num_nodes + i - 1) % num_nodes;
    (*curr_qs)[router_pos][ring_cw].insert(pair<uint64_t, EventPair>(curr_time + sw_to_sw_t, curr_q));
  }
  else
  {
    if (to_dir != NULL)
    {
      // to directory
      if (already_sent[target_port_num] == true)
      {
        return;
      }
      already_sent[target_port_num] = true;
      //curr_q.first->from.push(this);
      if (curr_q.first->dummy == true)
      {
        delete curr_q.first;
      }
      else if (queue_type == noc_req)
      {
        to_dir->add_req_event(curr_time + sw_to_sw_t, curr_q.first);
      }
      else
      {
        to_dir->add_rep_event(curr_time + sw_to_sw_t, curr_q.first);
      }
      num_req_in_ring--;
    }
    else
    {
      if (already_sent[target_port_num] == true)
      {
        return;
      }
      already_sent[target_port_num] = true;
      if (curr_q.first->dummy == true)
      {
        delete curr_q.first;
      }
      else
      { 
        //curr_q.first->from.push(this);
        to_l2->add_rep_event(curr_time + sw_to_sw_t, curr_q.first);
      }
      num_req_in_ring--;
    }
    if (queue_type == noc_crq)
    {
      curr_q.first->from.pop();
    }
  }
  (*curr_qs)[i][dir].erase((*curr_qs)[i][dir].begin());
  num_hops++;
}

