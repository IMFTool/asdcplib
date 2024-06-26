/*
Copyright (c) 2024 Wolfgang Ruppel, Robert Scheler, Heiko Sparenberg Fraunhofer IIS,
John Hurst, Pierre-Anthony Lemieux

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LMGASADMLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LMGASADMILITY, WHETHER IN CONTRACT, STRICT LMGASADMILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "AS_02_internal.h"
#include "AS_02_MGASADM.h"

#include <iostream>
#include <iomanip>
#include <stdexcept>
#include "AS_02_internal.h"

namespace Kumu {
  class RuntimeError : public std::runtime_error {
    Kumu::Result_t m_Result;
    RuntimeError();
  public:
    RuntimeError(const Kumu::Result_t& result) : std::runtime_error(result.Message()), m_Result(result) {}
    Kumu::Result_t GetResult() { return this->m_Result; }
    ~RuntimeError() throw() {}
  };
}

//
class AS_02::MGASADM::MXFWriter::h__Writer : public h__AS02Writer<AS_02::MXF::AS02IndexWriterVBR>
{
  ASDCP_NO_COPY_CONSTRUCT(h__Writer);
  h__Writer();
public:
    WriterState_t m_State;

  h__Writer(const Dictionary *d) : h__AS02Writer(d), m_State(ST_BEGIN) {}
  virtual ~h__Writer(){}
};

//
class AS_02::MGASADM::MXFReader::h__Reader : public h__AS02Reader
{
  ASDCP_NO_COPY_CONSTRUCT(h__Reader);
  h__Reader();
public:
  ReaderState_t m_State;

  h__Reader(const Dictionary *d, const Kumu::IFileReaderFactory& fileReaderFactory) :
    h__AS02Reader(d, fileReaderFactory), m_State(ST_READER_BEGIN) {}
  virtual ~h__Reader(){}
};

//------------------------------------------------------------------------------------------

/* Size of the BER Length of the clip */
static const int CLIP_BER_LENGTH_SIZE = 8;

/* Combined size of the Key and Length of the clip */
static const int RESERVED_KL_SIZE = ASDCP::SMPTE_UL_LENGTH + CLIP_BER_LENGTH_SIZE;


AS_02::MGASADM::MXFWriter::MXFWriter() : m_ClipStart(0), m_Writer(0) {
}

AS_02::MGASADM::MXFWriter::~MXFWriter() {}

const ASDCP::MXF::OP1aHeader&
AS_02::MGASADM::MXFWriter::OP1aHeader() const {
  if (this->m_Writer->m_State == ST_BEGIN) {
    throw Kumu::RuntimeError(Kumu::RESULT_INIT);
  }

  return this->m_Writer->m_HeaderPart;
}

const ASDCP::MXF::RIP&
AS_02::MGASADM::MXFWriter::RIP() const {
  if (this->m_Writer->m_State == ST_BEGIN) {
    throw Kumu::RuntimeError(Kumu::RESULT_INIT);
  }

  return this->m_Writer->m_RIP;
}

Kumu::Result_t
AS_02::MGASADM::MXFWriter::OpenWrite(
  const std::string& filename,
  const ASDCP::WriterInfo& Info,
  ASDCP::MXF::InterchangeObject_list_t& essence_sub_descriptor_list,
  const ASDCP::Rational& edit_rate,
  const ASDCP::Rational& sample_rate) {

  /* are we already running */

  if ( this->m_Writer && this->m_Writer->m_State != ST_BEGIN ) {
    KM_RESULT_STATE_HERE();
    return Kumu::RESULT_STATE;
  }

  Result_t result = Kumu::RESULT_OK;

  /* initialize the writer */

  this->m_Writer = new AS_02::MGASADM::MXFWriter::h__Writer(&DefaultSMPTEDict());

  this->m_Writer->m_Info = Info;

  this->m_Writer->m_HeaderSize = 16 * 1024;

  try {

    /* open the file */

    result = this->m_Writer->m_File.OpenWrite(filename.c_str());

    if (result.Failure()) {
      throw Kumu::RuntimeError(result);
    }

    /* initialize MGASADM descriptor */

    ASDCP::MXF::MGASoundEssenceDescriptor* desc = new ASDCP::MXF::MGASoundEssenceDescriptor(this->m_Writer->m_Dict);

    GenRandomValue(desc->InstanceUID); /* TODO: remove */
    desc->SampleRate = edit_rate;
    desc->AudioSamplingRate = sample_rate;
    desc->ChannelCount = 0;
    desc->SoundEssenceCoding = this->m_Writer->m_Dict->ul(MDD_MGAAudioEssenceUncompressedSoundCoding);
    desc->QuantizationBits = 24;

    this->m_Writer->m_EssenceDescriptor = desc;

    /* copy and add the MGASADM subdescriptors */

    ASDCP::MXF::InterchangeObject_list_t::iterator i;
    for ( i = essence_sub_descriptor_list.begin(); i != essence_sub_descriptor_list.end(); ++i ) {
        if ( ( (*i)->GetUL() != this->m_Writer->m_Dict->ul(MDD_MGASoundfield) )
                 && ( (*i)->GetUL() != this->m_Writer->m_Dict->ul(MDD_MGAAudioMetadataSubDescriptor) )
                 && ( (*i)->GetUL() != this->m_Writer->m_Dict->ul(MDD_SADMAudioMetadataSubDescriptor) )
                 && ( (*i)->GetUL() != this->m_Writer->m_Dict->ul(MDD_ContainerConstraintsSubDescriptor) )
  			  )
          {
            DefaultLogSink().Error("Essence sub-descriptor is not an MGA SubDescriptor.\n");
            (*i)->Dump();
          }

        this->m_Writer->m_EssenceSubDescriptorList.push_back(*i);
        if (!(*i)->InstanceUID.HasValue()) GenRandomValue((*i)->InstanceUID);
        this->m_Writer->m_EssenceDescriptor->SubDescriptors.push_back((*i)->InstanceUID);
        *i = 0; // parent will only free the ones we don't keep

    }

    /* initialize the index write */

    this->m_Writer->m_IndexWriter.SetEditRate(edit_rate);

    /* Essence Element UL */

    byte_t element_ul_bytes[ASDCP::SMPTE_UL_LENGTH];

    const ASDCP::MDDEntry& element_ul_entry = this->m_Writer->m_Dict->Type(MDD_ClipWrappedMGASADMSoundElement);

    std::copy(element_ul_entry.ul, element_ul_entry.ul + ASDCP::SMPTE_UL_LENGTH, element_ul_bytes);

    /* only a single track */

    element_ul_bytes[15] = 1;

    /* only a single element */

    element_ul_bytes[13] = 1;

    /* write the file header*/
    /* WriteAS02Header() takes ownership of desc and subdesc */

    result = this->m_Writer->WriteAS02Header(
      "Clip wrapping of MGA bitstreams as specified in SMPTE ST 2067-203",
      UL(this->m_Writer->m_Dict->ul(MDD_MXFGCClipWrappedMGA)),
      "MGA Bitstream",
      UL(element_ul_bytes),
      UL(this->m_Writer->m_Dict->ul(MDD_SoundDataDef)),
      edit_rate
    );

    if (result.Failure()) {
      throw Kumu::RuntimeError(result);
    }

    /* start the clip */

    this->m_ClipStart = this->m_Writer->m_File.TellPosition();

    /* reserve space for the KL of the KLV, which will be written later during finalization */

    byte_t clip_buffer[RESERVED_KL_SIZE] = { 0 };

    memcpy(clip_buffer, element_ul_bytes, ASDCP::SMPTE_UL_LENGTH);

    if (!Kumu::write_BER(clip_buffer + ASDCP::SMPTE_UL_LENGTH, 0, CLIP_BER_LENGTH_SIZE)) {
      throw Kumu::RuntimeError(Kumu::RESULT_FAIL);
    }

    result = this->m_Writer->m_File.Write(clip_buffer, RESERVED_KL_SIZE);

    if (result.Failure()) {
      throw Kumu::RuntimeError(result);
    }

    this->m_Writer->m_StreamOffset = RESERVED_KL_SIZE;

    this->m_Writer->m_State = ST_READY;

  } catch (Kumu::RuntimeError e) {

    this->Reset();

    return e.GetResult();

  }

  return result;

}

Result_t
AS_02::MGASADM::MXFWriter::WriteFrame(const ui8_t* frame, ui32_t sz) {
  /* are we running */

  if (this->m_Writer->m_State == ST_BEGIN) {
    return Kumu::RESULT_INIT;
  }

  if (sz == 0) {
    DefaultLogSink().Error("The frame buffer size is zero.\n");
    return RESULT_PARAM;
  }

  Result_t result = Kumu::RESULT_OK;

  /* update the index */
  IndexTableSegment::IndexEntry Entry;

  Entry.StreamOffset = this->m_Writer->m_StreamOffset;

  this->m_Writer->m_IndexWriter.PushIndexEntry(Entry);

  /* write the frame */

  result = this->m_Writer->m_File.Write(frame, sz);

  if (result.Failure()) {
    this->Reset();
    return result;
  }

  /* increment the frame counter */

  this->m_Writer->m_FramesWritten++;

  /* increment stream offset */

  this->m_Writer->m_StreamOffset += sz;

  /* we are running now */

  this->m_Writer->m_State = ST_RUNNING;

  return result;
}

Result_t AS_02::MGASADM::MXFWriter::WriteFrame(const ASDCP::FrameBuffer& frame) {
  return WriteFrame(frame.RoData(), frame.Size());
}

Result_t
AS_02::MGASADM::MXFWriter::Finalize() {

  /* are we running */

  if (this->m_Writer->m_State == ST_BEGIN) {
    return Kumu::RESULT_INIT;
  }
  if (this->m_Writer->m_State != ST_RUNNING) {
    KM_RESULT_STATE_HERE();
    return RESULT_STATE;
  }


  Result_t result = RESULT_OK;

  try {

    /* write clip length */

    ui64_t current_position = this->m_Writer->m_File.TellPosition();

    result = this->m_Writer->m_File.Seek(m_ClipStart + ASDCP::SMPTE_UL_LENGTH);

    byte_t clip_buffer[CLIP_BER_LENGTH_SIZE] = { 0 };

    ui64_t size = static_cast<ui64_t>(this->m_Writer->m_StreamOffset) /* total size of the KLV */ - RESERVED_KL_SIZE;

    bool check = Kumu::write_BER(clip_buffer, size, CLIP_BER_LENGTH_SIZE);

    if (!check) {
      throw Kumu::RuntimeError(Kumu::RESULT_FAIL);
    }

    result = this->m_Writer->m_File.Write(clip_buffer, CLIP_BER_LENGTH_SIZE);

    if (result.Failure()) {
      throw Kumu::RuntimeError(result);
    }

    result = this->m_Writer->m_File.Seek(current_position);

    if (result.Failure()) {
      throw Kumu::RuntimeError(result);
    }

    /* write footer */

    result = this->m_Writer->WriteAS02Footer();

    if (result.Failure()) {
      throw Kumu::RuntimeError(result);
    }

  } catch (Kumu::RuntimeError e) {

    /* nothing to do since we are about to reset */

    result = e.GetResult();

  }

  /* we are ready to start again */

  this->Reset();

  return result;
}

void
AS_02::MGASADM::MXFWriter::Reset() {
  this->m_Writer.set(0);
}


//------------------------------------------------------------------------------------------


AS_02::MGASADM::MXFReader::MXFReader(const Kumu::IFileReaderFactory& fileReaderFactory) :
  m_FileReaderFactory(fileReaderFactory), m_Reader(0) {}

AS_02::MGASADM::MXFReader::~MXFReader() {
    if ( m_Reader && m_Reader->m_File->IsOpen()) {
      m_Reader->Close();
    }
}

ASDCP::MXF::OP1aHeader&
AS_02::MGASADM::MXFReader::OP1aHeader() const {
  if (this->m_Reader->m_State == ST_READER_BEGIN) {
    throw Kumu::RuntimeError(Kumu::RESULT_INIT);
  }

  return this->m_Reader->m_HeaderPart;
}

const ASDCP::MXF::RIP&
AS_02::MGASADM::MXFReader::RIP() const {
  if (this->m_Reader->m_State == ST_READER_BEGIN) {
    throw Kumu::RuntimeError(Kumu::RESULT_INIT);
  }

  return this->m_Reader->m_RIP;
}

Result_t
AS_02::MGASADM::MXFReader::OpenRead(const std::string& filename) {

  /* are we already running */

  if ( this->m_Reader && this->m_Reader->m_State != ST_READER_BEGIN ) {
    KM_RESULT_STATE_HERE();
    return Kumu::RESULT_STATE;
  }

  Result_t result = Kumu::RESULT_OK;

  /* initialize the writer */

  this->m_Reader = new h__Reader(&DefaultCompositeDict(), m_FileReaderFactory);

  try {

    result = this->m_Reader->OpenMXFRead(filename);

    InterchangeObject* tmp_iobj = 0;

    if ( ASDCP_SUCCESS(result)){
      this->m_Reader->m_HeaderPart.GetMDObjectByType(
        this->m_Reader->m_Dict->Type(MDD_MGASoundEssenceDescriptor).ul,
        &tmp_iobj
    );
    }

    if (!tmp_iobj) {
      DefaultLogSink().Error("MGASoundEssenceDescriptor object not found in IMF/MGASADM MXF file.\n");
      throw Kumu::RuntimeError(Kumu::RESULT_FAIL);
    }

    std::list<InterchangeObject*> ObjectList;

    this->m_Reader->m_HeaderPart.GetMDObjectsByType(
      this->m_Reader->m_Dict->Type(MDD_MGASoundfieldGroupLabelSubDescriptor).ul,
      ObjectList
    );

    if (ObjectList.empty()) {
      DefaultLogSink().Error("MGASoundfieldGroupLabelSubDescriptor object not found.\n");
      throw Kumu::RuntimeError(Kumu::RESULT_FAIL);
    }

    this->m_Reader->m_HeaderPart.GetMDObjectsByType(
      this->m_Reader->m_Dict->Type(MDD_Track).ul,
      ObjectList
    );

    if (ObjectList.empty()) {
      throw Kumu::RuntimeError(Kumu::RESULT_FAIL);
    }

    /* invalidate current frame */

    /* we are ready */

    this->m_Reader->m_State = ST_READER_READY;

  } catch (Kumu::RuntimeError e) {

    this->Reset();

    return e.GetResult();
  }

  return RESULT_OK;
}


Result_t
AS_02::MGASADM::MXFReader::Close() {

  /* are we already running */

  if (this->m_Reader->m_State == ST_READER_BEGIN) {
    return Kumu::RESULT_INIT;
  }

  this->Reset();

  return Kumu::RESULT_OK;
}


Result_t AS_02::MGASADM::MXFReader::GetFrameCount(ui32_t& frameCount) const {

  /* are we already running */

  if (this->m_Reader->m_State == ST_READER_BEGIN) {
    return Kumu::RESULT_INIT;
  }

  frameCount = m_Reader->m_IndexAccess.GetDuration();

  return Kumu::RESULT_OK;
}

/* Anonymous namespace with ReadFrame helpers */
namespace {
  bool checkFrameCapacity(ASDCP::FrameBuffer& frame, size_t size, bool reallocate_if_needed) {

    if (frame.Capacity() < size) {
      if (!reallocate_if_needed) {
        return false;
      }
      Result_t result = frame.Capacity(size);
      return result == RESULT_OK;
    }
    return true;
  }

  Result_t
  ReadFrameImpl(ui32_t frame_number, ASDCP::FrameBuffer& frame, ReaderState_t& reader_state, AS_02::h__AS02Reader *reader, bool reallocate_if_needed) {
    assert(reader);
    /* are we already running */

    if (reader_state == ST_READER_BEGIN) {
      return Kumu::RESULT_INIT;
    }

    Result_t result = RESULT_OK;

    // look up frame index node
    IndexTableSegment::IndexEntry index_entry;

    result = reader->m_IndexAccess.Lookup(frame_number, index_entry);

    if (result.Failure()) {
      DefaultLogSink().Error("Frame value out of range: %u\n", frame_number);
      return result;
    }

    result = reader->m_File->Seek(index_entry.StreamOffset);

    if (result.Failure()) {
      DefaultLogSink().Error("Cannot seek to stream offset: %u\n", index_entry.StreamOffset);
      return result;
    }

    /* read the preamble info */

    const int preambleTLLen = 7;

    ui32_t buffer_offset = 0;

    if (!checkFrameCapacity(frame, preambleTLLen, reallocate_if_needed)) {
        return RESULT_SMALLBUF;
    }

    result = reader->m_File->Read(&frame.Data()[buffer_offset], preambleTLLen);

    if (result.Failure()) {
      DefaultLogSink().Error("Error reading MGA Frame preamble\n", index_entry.StreamOffset);
      return result;
    }

    ui32_t audioLen = ((ui32_t)frame.Data()[3 + buffer_offset] << 24) +
      ((ui32_t)frame.Data()[4 + buffer_offset] << 16) +
      ((ui32_t)frame.Data()[5 + buffer_offset] << 8) +
      (ui32_t)frame.Data()[6 + buffer_offset];


    /* read the preamble*/

    if (audioLen > 0) {

      result = reader->m_File->Seek(audioLen, Kumu::SP_POS);

      if (result.Failure()) {
        DefaultLogSink().Error("Error Seeking\n");
        return result;
      }

    }
    const int preambleMetaLen = 6;

    result = reader->m_File->Read(&frame.Data()[buffer_offset], preambleMetaLen);

    if (result.Failure()) {
      DefaultLogSink().Error("Error reading MGA Frame preamble\n", index_entry.StreamOffset);
      return result;
    }

    ui32_t metaLen = ((ui32_t)frame.Data()[2 + buffer_offset] << 24) +
      ((ui32_t)frame.Data()[3 + buffer_offset] << 16) +
      ((ui32_t)frame.Data()[4 + buffer_offset] << 8) +
      (ui32_t)frame.Data()[5 + buffer_offset];

    result = reader->m_File->Seek(6, Kumu::SP_POS);
    metaLen -= 6;
    result = reader->m_File->Read(&frame.Data()[buffer_offset], metaLen);
    if ( KM_SUCCESS(result) )
	{
    	frame.Size(metaLen);
	}
    if (result.Failure()) {
      DefaultLogSink().Error("Error reading MGA metadata\n", index_entry.StreamOffset);
      return result;
    }


    reader_state = ST_READER_RUNNING;

    return result;
  }

  Result_t
  ReadMetaFrameImpl(ui32_t frame_number, ASDCP::FrameBuffer& frame, ReaderState_t& reader_state, AS_02::h__AS02Reader *reader, bool reallocate_if_needed) {
    assert(reader);
    /* are we already running */

    if (reader_state == ST_READER_BEGIN) {
      return Kumu::RESULT_INIT;
    }

    Result_t result = RESULT_OK;

    // look up frame index node
    IndexTableSegment::IndexEntry index_entry;

    result = reader->m_IndexAccess.Lookup(frame_number, index_entry);

    if (result.Failure()) {
      DefaultLogSink().Error("Frame value out of range: %u\n", frame_number);
      return result;
    }

    result = reader->m_File->Seek(index_entry.StreamOffset);

    if (result.Failure()) {
      DefaultLogSink().Error("Cannot seek to stream offset: %u\n", index_entry.StreamOffset);
      return result;
    }

    /* read the preamble info */

    const int preambleLen = 7;

    if (!checkFrameCapacity(frame, preambleLen, reallocate_if_needed)) {
        return RESULT_SMALLBUF;
    }

    result = reader->m_File->Read(frame.Data(), preambleLen);

    if (result.Failure()) {
      DefaultLogSink().Error("Error reading MGA Frame preamble\n", index_entry.StreamOffset);
      return result;
    }

    ui32_t audioLen = ((ui32_t)frame.Data()[3] << 24) +
      ((ui32_t)frame.Data()[4] << 16) +
      ((ui32_t)frame.Data()[5] << 8) +
      (ui32_t)frame.Data()[6];


    /* read the preamble*/

    if (audioLen > 0) {

      result = reader->m_File->Seek(audioLen, Kumu::SP_POS);

      if (result.Failure()) {
        DefaultLogSink().Error("Error Seeking\n");
        return result;
      }

    }
    const int preambleMetaLen = 6;

    result = reader->m_File->Read(frame.Data(), preambleMetaLen);

    if (result.Failure()) {
      DefaultLogSink().Error("Error reading MGA Frame preamble\n", index_entry.StreamOffset);
      return result;
    }

    ui32_t metaLen = ((ui32_t)frame.Data()[2] << 24) +
      ((ui32_t)frame.Data()[3] << 16) +
      ((ui32_t)frame.Data()[4] << 8) +
      (ui32_t)frame.Data()[5];

    if (!checkFrameCapacity(frame, metaLen, reallocate_if_needed)) {
        return RESULT_SMALLBUF;
    }

    result = reader->m_File->Read(frame.Data(), metaLen);
    if ( KM_SUCCESS(result) )
	{
    	frame.Size(metaLen);
	}

    if (result.Failure()) {
      DefaultLogSink().Error("Error reading MGA metadata\n", index_entry.StreamOffset);
      return result;
    }


    reader_state = ST_READER_RUNNING;

    return result;
  }
} // namespace

Result_t AS_02::MGASADM::MXFReader::ReadFrame(ui32_t frame_number,
                                          AS_02::MGASADM::MXFReader::Frame &frame) {
  assert(!this->m_Reader.empty());
  Result_t result = ReadFrameImpl(frame_number, this->m_FrameBuffer,
                                  this->m_Reader->m_State, this->m_Reader, true);

  frame = std::pair<size_t, const ui8_t *>(this->m_FrameBuffer.Size(),
                                           this->m_FrameBuffer.Data());
  return result;
}

Result_t AS_02::MGASADM::MXFReader::ReadFrame(ui32_t frame_number,
                                          ASDCP::FrameBuffer &frame) {
  return ReadFrameImpl(frame_number, frame, this->m_Reader->m_State, this->m_Reader,
                       false);
}

Result_t AS_02::MGASADM::MXFReader::ReadMetaFrame(ui32_t frame_number,
                                          ASDCP::FrameBuffer &frame) {
  return ReadMetaFrameImpl(frame_number, frame, this->m_Reader->m_State, this->m_Reader, false);
}

Result_t AS_02::MGASADM::MXFReader::FillWriterInfo(WriterInfo& Info) const {
  /* are we already running */

  if (this->m_Reader->m_State == ST_READER_BEGIN) {
    return Kumu::RESULT_FAIL;
  }

  Info = m_Reader->m_Info;

  return Kumu::RESULT_OK;
}

void
AS_02::MGASADM::MXFReader::DumpHeaderMetadata(FILE* stream) const {
  if (this->m_Reader->m_State != ST_READER_BEGIN) {
    this->m_Reader->m_HeaderPart.Dump(stream);
  }
}


void
AS_02::MGASADM::MXFReader::DumpIndex(FILE* stream) const {
  if (this->m_Reader->m_State != ST_READER_BEGIN) {
    this->m_Reader->m_IndexAccess.Dump(stream);
  }
}

void
AS_02::MGASADM::MXFReader::Reset() {
  if ( m_Reader && m_Reader->m_File->IsOpen()) {
    m_Reader->Close();
  }

  this->m_Reader.set(0);
}

//
// end AS_02_MGASADM.cpp
//
