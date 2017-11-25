/*
KVMGFX Client - A KVM Client for VGA Passthrough
Copyright (C) 2017 Geoffrey McRae <geoff@hostfission.com>

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include "Service.h"
#include "IVSHMEM.h"

#include "common\debug.h"
#include "common\KVMGFXHeader.h"

#include "CaptureFactory.h"

Service * Service::m_instance = NULL;

Service::Service() :
  m_initialized(false),
  m_readyEvent(INVALID_HANDLE_VALUE),
  m_capture(NULL),
  m_memory(NULL),
  m_frameIndex(0)
{
  m_ivshmem = IVSHMEM::Get();
}

Service::~Service()
{
}

bool Service::Initialize(ICapture * captureDevice)
{
  if (m_initialized)
    DeInitialize();

  m_capture = captureDevice;
  if (!m_ivshmem->Initialize())
  {
    DEBUG_ERROR("IVSHMEM failed to initalize");
    DeInitialize();
    return false;
  }

  if (m_ivshmem->GetSize() < sizeof(KVMGFXHeader))
  {
    DEBUG_ERROR("Shared memory is not large enough for the KVMGFXHeader");
    DeInitialize();
    return false;
  }

  m_memory = static_cast<uint8_t*>(m_ivshmem->GetMemory());
  if (!m_memory)
  {
    DEBUG_ERROR("Failed to get IVSHMEM memory");
    DeInitialize();
    return false;
  }

  m_readyEvent = m_ivshmem->CreateVectorEvent(0);
  if (m_readyEvent == INVALID_HANDLE_VALUE)
  {
    DEBUG_ERROR("Failed to get event for vector 0");
    DeInitialize();
    return false;
  }

  KVMGFXHeader * header = reinterpret_cast<KVMGFXHeader*>(m_memory);

  // we save this as it might actually be valid
  UINT16 hostID = header->hostID;

  ZeroMemory(header, sizeof(KVMGFXHeader));
  memcpy(header->magic, KVMGFX_HEADER_MAGIC, sizeof(KVMGFX_HEADER_MAGIC));

  header->version   = KVMGFX_HEADER_VERSION;
  header->guestID   = m_ivshmem->GetPeerID();
  header->hostID    = hostID;

  m_initialized = true;
  return true;
}

void Service::DeInitialize()
{
  if (m_readyEvent != INVALID_HANDLE_VALUE)
    CloseHandle(m_readyEvent);

  m_memory = NULL;
  m_ivshmem->DeInitialize();

  if (m_capture)
  {
    m_capture->DeInitialize();
    m_capture = NULL;
  }

  m_initialized = false;
}

bool Service::Process()
{
  if (!m_initialized)
    return false;

  KVMGFXHeader * header = reinterpret_cast<KVMGFXHeader *>(m_memory);

  // calculate the current offset and ensure it is 16-byte aligned for SMID performance
  uint64_t dataOffset = sizeof(KVMGFXHeader) + m_frameIndex * m_capture->GetMaxFrameSize();
  dataOffset = (dataOffset + 0xF) & ~0xF;

  uint8_t      * data       = m_memory + dataOffset;
  const size_t   available  = m_ivshmem->GetSize() - sizeof(KVMGFXHeader);

  if (dataOffset + m_capture->GetMaxFrameSize() > available)
  {
    DEBUG_ERROR("Frame can exceed buffer size!");
    return false;
  }

  // setup the header
  header->frameType = m_capture->GetFrameType();
  header->dataLen   = 0;

  FrameInfo frame;
  frame.buffer     = data;
  frame.bufferSize = m_ivshmem->GetSize() - sizeof(KVMGFXHeader);

  // capture a frame of data
  if (!m_capture->GrabFrame(frame))
  {
    header->dataLen = 0;
    DEBUG_ERROR("Capture failed");
    return false;
  }

  // wait for the host to notify that is it is ready to proceed
  bool eventDone = false;
  while (!eventDone)
  {
    switch (WaitForSingleObject(m_readyEvent, 1000))
    {
    case WAIT_ABANDONED:
      DEBUG_ERROR("Wait abandoned");
      return false;

    case WAIT_OBJECT_0:
      eventDone = true;
      break;

      // if we timed out we just continue to ring until we get an answer or we are stopped
    case WAIT_TIMEOUT:
      continue;

    case WAIT_FAILED:
      DEBUG_ERROR("Wait failed");
      return false;

    default:
      DEBUG_ERROR("Unknown error");
      return false;
    }
  }

  // copy the frame details into the header
  header->width   = frame.width;
  header->height  = frame.height;
  header->stride  = frame.stride;
  header->dataPos = dataOffset;
  header->dataLen = frame.outSize;

  // tell the host where the cursor is
  POINT cursorPos;
  GetCursorPos(&cursorPos);
  header->mouseX = cursorPos.x;
  header->mouseY = cursorPos.y;

  ResetEvent(m_readyEvent);
  if (!m_ivshmem->RingDoorbell(header->hostID, 0))
  {
    DEBUG_ERROR("Failed to ring doorbell");
    return false;
  }

  if (++m_frameIndex == 2)
    m_frameIndex = 0;

  return true;
}