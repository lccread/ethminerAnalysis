/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	cpp-ethereum is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with cpp-ethereum.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file Worker.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Worker.h"

#include <chrono>
#include <thread>
#include "Log.h"
using namespace std;
using namespace dev;

void Worker::startWorking()
{
//	cnote << "startWorking for thread" << m_name;
	Guard l(x_work);
	if (m_work)
	{
		WorkerState ex = WorkerState::Stopped;
		m_state.compare_exchange_strong(ex, WorkerState::Starting);
	}
	else
	{
		m_state = WorkerState::Starting;
		//创建工作线程
		m_work.reset(new thread([&]()
		{
			setThreadName(m_name.c_str());
//			cnote << "Thread begins";
			while (m_state != WorkerState::Killing)
			{
				WorkerState ex = WorkerState::Starting;
				bool ok = m_state.compare_exchange_strong(ex, WorkerState::Started);
//				cnote << "Trying to set Started: Thread was" << (unsigned)ex << "; " << ok;
				(void)ok;

				try
				{
					//工作循环，派生类实现的函数
					workLoop();
				}
				catch (std::exception const& _e)
				{
					clog(WarnChannel) << "Exception thrown in Worker thread: " << _e.what();
				}

//				ex = WorkerState::Stopping;
//				m_state.compare_exchange_strong(ex, WorkerState::Stopped);

				ex = m_state.exchange(WorkerState::Stopped);
//				cnote << "State: Stopped: Thread was" << (unsigned)ex;
				if (ex == WorkerState::Killing || ex == WorkerState::Starting)
					m_state.exchange(ex);

				while (m_state == WorkerState::Stopped)
					this_thread::sleep_for(chrono::milliseconds(20));
			}
		}));
//		cnote << "Spawning" << m_name;
	}
	while (m_state == WorkerState::Starting)
		this_thread::sleep_for(chrono::microseconds(20));
}

void Worker::stopWorking()
{
	DEV_GUARDED(x_work)
		if (m_work)
		{
			WorkerState ex = WorkerState::Started;
			m_state.compare_exchange_strong(ex, WorkerState::Stopping);

			while (m_state != WorkerState::Stopped)
				this_thread::sleep_for(chrono::microseconds(20));
		}
}

Worker::~Worker()
{
	DEV_GUARDED(x_work)
		if (m_work)
		{
			m_state.exchange(WorkerState::Killing);
			m_work->join();
			m_work.reset();
		}
}
