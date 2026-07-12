/*
    SPDX-FileCopyrightText: 2025-2026 Jakub Stankowski <jakub.stankowski@put.poznan.pl>
    SPDX-FileCopyrightText: 2026 Błażej Szydełko <blazej.szydelkoi@put.poznan.pl>
    SPDX-License-Identifier: BSD-3-Clause
*/
#include "xPly.h"
#include "xMemory.h"
#include "xSplatOps.h"
#include "fmt/format.h"
#include "xFileListUtils.h"
#include <iostream>
#include <sstream>
#include <strstream>
#include <unordered_map>
#include "xString.h"

namespace PMBB_NAMESPACE::GSC {

//===============================================================================================================================================================================================================

xPly2::tRes xPly2::readGC(xGaussianCloud* DstCloud, xStream* Stream, bool HeaderOnly)
{
  if(!Stream->canRead()) { return { eRetv::Error, "Stream does not allow Read" }; }

  m_CameraData.clear();
  m_NumSplats        = NOT_VALID;
  m_NumSphHarmCoeffs = 0;
  m_NumSphHarm       = 0;
  m_PropertyOffset.clear();
  m_AttrIdToOffset.clear();

  std::string Header = xFindHeader(Stream);
  if(Header.empty()) { return { eRetv::Error, "PLY header cannot be found !" }; }

  xInOutResult HeaderRes = xParseHeaderGSC(Header);
  if(!HeaderRes) { return HeaderRes; }

  const int32 SphHarmDegree = xSplatOps::xSphHarmDegreeFromCount(m_NumSphHarm);
  const int32 NumProperties = (int32)m_PropertyOffset.size();

  
  DstCloud->setNumSplats    (m_NumSplats  );
  DstCloud->setCameraData   (m_CameraData );
  DstCloud->setSphHarmDegree(SphHarmDegree);
  DstCloud->setSphHarmCount (m_NumSphHarm );
  DstCloud->setAttCount     ((uint32)m_PropertyOffset.size());
  
  if(HeaderOnly) { return eRetv::Success; }

  DstCloud->create(m_NumSplats);
  //read cloud
  std::vector<flt32> TmpBuff(c_NumSplatsInBatch * NumProperties, std::numeric_limits<flt32>::quiet_NaN());

  for(int32 BatchSplatIdx = 0; BatchSplatIdx < m_NumSplats; BatchSplatIdx += c_NumSplatsInBatch)
  {
    int32 NumSplatsInCurrBatch = xMin(c_NumSplatsInBatch, m_NumSplats - BatchSplatIdx);
    bool ReadR = Stream->read(TmpBuff.data(), NumSplatsInCurrBatch * NumProperties * sizeof(flt32));
    if(!ReadR) { return { eRetv::Error, "PLY cloud read error" }; }
    
    for(uint32 AttrIdx = 0; AttrIdx < (uint32)xAttrFLT::COUNT; AttrIdx++)
    {
      const xAttrFLT  AttrId     = (xAttrFLT)AttrIdx;
      const uint32    AttrOffset = m_AttrIdToOffset[AttrId];
      const flt32*    SrcPtr     = TmpBuff.data() + AttrOffset;
      flt32* restrict DstPtr     = DstCloud->getAttrAddr(AttrId);
      for(int32 SrcSplatIdx = 0, DstSplatIdx = BatchSplatIdx; SrcSplatIdx < NumSplatsInCurrBatch; SrcSplatIdx++, DstSplatIdx++)
      {        
        flt32 AttrVal = SrcPtr[SrcSplatIdx * NumProperties];
        DstPtr[DstSplatIdx] = AttrVal;
      }
    }
  }

  return eRetv::Success;
}
xPly2::tRes xPly2::writeGC(xStream* Stream, const xGaussianCloud* SrcCloud)
{
  const int32 NumSplats     = SrcCloud->getNumSplats();
  const int32 NumSphHarm    = xSplatOps::xSphHarmCountFromDegree(SrcCloud->getSphHarmDegree());
  const int32 NumProperties = 14 + NumSphHarm * 3;

  std::string Header = xFormatHeaderGSC(NumSplats, NumSphHarm);
  //fmt::print(Header);
  Stream->write(Header);

  //a bit dirty trick, but saves a lot of effort with building offset tables
  xParseHeaderGSC(Header);

  assert(m_NumSplats                    == NumSplats    );
  assert((int32)m_PropertyOffset.size() == NumProperties);
  assert(m_NumSphHarm                   == NumSphHarm   );

  std::vector<flt32> TmpBuff(c_NumSplatsInBatch * NumProperties, std::numeric_limits<flt32>::quiet_NaN());

  for(int32 BatchSplatIdx = 0; BatchSplatIdx < m_NumSplats; BatchSplatIdx += c_NumSplatsInBatch)
  {
    int32 NumSplatsInCurrBatch = xMin(c_NumSplatsInBatch, m_NumSplats - BatchSplatIdx);

    for(uint32 AttrIdx = 0; AttrIdx < (uint32)xAttrFLT::COUNT; AttrIdx++)
    {
      const xAttrFLT  AttrId     = (xAttrFLT)AttrIdx;
      const uint32    AttrOffset = m_AttrIdToOffset[AttrId];
      const flt32*    SrcPtr     = SrcCloud->getAttrAddr(AttrId);
      flt32* restrict DstPtr     = TmpBuff.data() + AttrOffset;
      for(int32 DstSplatIdx = 0, SrcSplatIdx = BatchSplatIdx; DstSplatIdx < NumSplatsInCurrBatch; DstSplatIdx++, SrcSplatIdx++)
      {
        flt32 AttrVal = SrcPtr[SrcSplatIdx];
        DstPtr[DstSplatIdx * NumProperties] = AttrVal;
      }
    }

    ////dirty hack for unused nx, ny, and nz
    //for(uint32 FakeOffset = 3; FakeOffset < 6; FakeOffset++)
    //{
    //  flt32* restrict DstPtr = TmpBuff.data() + FakeOffset;
    //  for(int32 DstSplatIdx = 0, SrcSplatIdx = BatchSplatIdx; DstSplatIdx < NumSplatsInCurrBatch; DstSplatIdx++, SrcSplatIdx++)
    //  {
    //    DstPtr[DstSplatIdx * NumProperties] = 0.0f;
    //  }
    //}

    Stream->write(TmpBuff.data(), NumSplatsInCurrBatch * NumProperties * sizeof(flt32));
  }

  return eRetv::Success;
}
xPly2::tRes xPly2::writePC(xStream* Stream, const xPointCloud* SrcCloud, bool Binary)
{
  const int32 NumPoints = SrcCloud->getNumValidPoints();

  const xPointCloud::tCoordEl* CoordsPtr = SrcCloud->getCoordsPtrRaw();
  const xPointCloud::tColorEl* ColorsPtr = SrcCloud->getColorsPtrRaw();

  std::string Header = xFormatHeaderPC(NumPoints, Binary);
  Stream->write(Header);

  if(Binary)
  {
    const int32 NumProperties = 6;
    std::vector<flt32> TmpBuff(c_NumPointsInBatch * NumProperties, std::numeric_limits<flt32>::quiet_NaN());

    flt32* restrict DstPtr = TmpBuff.data();

    for(int32 BatchPointIdx = 0; BatchPointIdx < NumPoints; BatchPointIdx += c_NumPointsInBatch)
    {
      int32 NumPointsInCurrBatch = xMin(c_NumPointsInBatch, NumPoints - BatchPointIdx);
      for(int32 DstPointIdx = 0, SrcPointIdx = BatchPointIdx; DstPointIdx < NumPointsInCurrBatch; DstPointIdx++, SrcPointIdx++)
      {
        DstPtr[DstPointIdx * NumProperties + 0] = CoordsPtr[(SrcPointIdx << 2) + 0];
        DstPtr[DstPointIdx * NumProperties + 1] = CoordsPtr[(SrcPointIdx << 2) + 1];
        DstPtr[DstPointIdx * NumProperties + 2] = CoordsPtr[(SrcPointIdx << 2) + 2];
        DstPtr[DstPointIdx * NumProperties + 3] = ColorsPtr[(SrcPointIdx << 2) + 0]  * 255;
        DstPtr[DstPointIdx * NumProperties + 4] = ColorsPtr[(SrcPointIdx << 2) + 1]  * 255;
        DstPtr[DstPointIdx * NumProperties + 5] = ColorsPtr[(SrcPointIdx << 2) + 2]  * 255;
      }
      Stream->write(TmpBuff.data(), NumPointsInCurrBatch * NumProperties * sizeof(flt32));
    }
  }
  else
  {
    std::string Buffer; Buffer.reserve(xMax(xMemory::getBestEffortSizePageBase() * 64, xMemory::getRealSizePageHuge()));
    for(int32 i = 0; i < NumPoints; i++)
    {
      fmt::format_to(std::back_inserter(Buffer), "{0:12f} {1:12f} {2:12f} {3:12f} {4:12f} {5:12f}\n",
        CoordsPtr[(i << 2) + 0],
        CoordsPtr[(i << 2) + 1],
        CoordsPtr[(i << 2) + 2],
        ColorsPtr[(i << 2) + 0],
        ColorsPtr[(i << 2) + 1],
        ColorsPtr[(i << 2) + 2]);

      if(Buffer.size() > Buffer.capacity() - xMemory::getBestEffortSizePageBase())
      {
        Stream->write(Buffer);
        Buffer.clear();
      }
    }
  }

  return eRetv::Success;
}

std::string xPly2::xFindHeader(xStream* Stream)
{
  static constexpr int64 ReadSize = 4096*16;
  const int64 FileSize = Stream->size();
  std::string TmpBuffer; TmpBuffer.resize(ReadSize);
  int64 NumRead1st = xMin((int64)TmpBuffer.size(), FileSize);
  Stream->read(TmpBuffer.data(), NumRead1st);
  int64 EndPos = TmpBuffer.find("end_header");;
  while(EndPos == NOT_VALID)
  {
    int64 CurrSize = TmpBuffer.size();
    TmpBuffer.resize(CurrSize + ReadSize);
    int64 NumReadNth = xMin((int64)TmpBuffer.size(), FileSize) - CurrSize;
    if(NumReadNth == 0) { return ""; };
    Stream->read(TmpBuffer.data(), NumRead1st);
    EndPos = TmpBuffer.find("end_header");;
  }
  EndPos = EndPos + 11;
  if(TmpBuffer[EndPos] == '\r' && TmpBuffer[EndPos+1] == '\n') //CRLF file
  {
    EndPos++;
  }
  TmpBuffer.resize(EndPos);
  //fmt::print(TmpBuffer);
  Stream->seekR(EndPos, xStream::eSeek::Beg);
  return TmpBuffer;
}
int32 xPly2::xFindPropertyOffset(const std::string& AttributeName)
{
  const auto& Itr = m_PropertyOffset.find(AttributeName);
  if(Itr == m_PropertyOffset.end()) { return NOT_VALID; }
  return Itr->second;
}
xSeqFile::tResult xPly2::xParseHeaderGSC(const std::string& Header)
{
  std::stringstream HeaderStream;
  HeaderStream.str(Header);

  std::string Line;

  //1st line have to contain "ply"
  std::getline(HeaderStream, Line);
  if(Line != "ply") { return { eRetv::Error, "not a .ply file" }; }

  //try next line
  std::istream &Result = std::getline(HeaderStream, Line);
  if(!Result) { return { eRetv::Error, "missing header data" }; }

  //try parse comments
  while(Line.find("comment") == 0)
  {
    if(Line.find("camera_position_header") != std::string::npos) //camera position header
    {
      std::vector<std::string> HeadLineSplit = xString::split(Line, ' ');
      //TODO read this line to get information about location of each parameter

      std::getline(HeaderStream, Line);
      while(Line.find("camera_position") != std::string::npos)
      {
        std::vector<std::string> LineSplit = xString::split(Line, ' ');

        int32 ImgIdx = std::stoi(LineSplit[ 2]);
        int32 CamIdx = std::stoi(LineSplit[ 3]);
        flt64 PosX   = std::stod(LineSplit[ 4]);
        flt64 PosY   = std::stod(LineSplit[ 5]);
        flt64 PosZ   = std::stod(LineSplit[ 6]);
        flt64 QuatW  = std::stod(LineSplit[ 7]);
        flt64 QuatX  = std::stod(LineSplit[ 8]);
        flt64 QuatY  = std::stod(LineSplit[ 9]);
        flt64 QuatZ  = std::stod(LineSplit[10]);
        flt64 FocalX = std::stod(LineSplit[11]);
        flt64 FocalY = std::stod(LineSplit[12]);
        tStr  Name   = LineSplit[13];

        m_CameraData.emplace_back(ImgIdx, CamIdx, flt64V3{ PosX , PosY, PosZ }, flt64V4{ QuatW, QuatX, QuatY, QuatZ, }, flt64V2{ FocalX, FocalY }, Name);

        std::getline(HeaderStream, Line);
      }
    }
    else
    {
      return { eRetv::Error, "unknown comment" };
    }
    if(Line.find("comment") == 0) { std::getline(HeaderStream, Line); }
  }

  if(Line != "format binary_little_endian 1.0") { return { eRetv::Error, "only binary_little_endian 1.0 format is accepted" }; }

  std::getline(HeaderStream, Line);
  if(Line.find("element vertex ") != 0) { return { eRetv::Error, "missing vertex count" }; }

  m_NumSplats = std::stoi(Line.substr(std::strlen("element vertex ")));

  if(m_NumSplats <= 0) { return { eRetv::Error, fmt::format("invalid vertex count: {}", m_NumSplats) }; }

  // Parse attribute names
  for(int32 i = 0; ; i++)
  {
    std::getline(HeaderStream, Line);
    if(Line == "end_header") { break; }
    if(Line.substr(0, c_PropertyPatternLen) != c_PropertyPattern) { return { eRetv::Error, fmt::format("unsupported property: [{}]", Line) }; }
    std::string AttrStr = Line.substr(c_PropertyPatternLen);
    m_PropertyOffset[AttrStr] = i;
  }

  m_AttrIdToOffset[xAttrFLT::PX ] = xFindPropertyOffset("x"      );
  m_AttrIdToOffset[xAttrFLT::PY ] = xFindPropertyOffset("y"      );
  m_AttrIdToOffset[xAttrFLT::PZ ] = xFindPropertyOffset("z"      );
  m_AttrIdToOffset[xAttrFLT::SX ] = xFindPropertyOffset("scale_0");
  m_AttrIdToOffset[xAttrFLT::SY ] = xFindPropertyOffset("scale_1");
  m_AttrIdToOffset[xAttrFLT::SZ ] = xFindPropertyOffset("scale_2");
  m_AttrIdToOffset[xAttrFLT::RX ] = xFindPropertyOffset("rot_1"  );
  m_AttrIdToOffset[xAttrFLT::RY ] = xFindPropertyOffset("rot_2"  );
  m_AttrIdToOffset[xAttrFLT::RZ ] = xFindPropertyOffset("rot_3"  );
  m_AttrIdToOffset[xAttrFLT::RW ] = xFindPropertyOffset("rot_0"  );
  m_AttrIdToOffset[xAttrFLT::OP ] = xFindPropertyOffset("opacity");
  m_AttrIdToOffset[xAttrFLT::HR0] = xFindPropertyOffset("f_dc_0" );
  m_AttrIdToOffset[xAttrFLT::HG0] = xFindPropertyOffset("f_dc_1" );
  m_AttrIdToOffset[xAttrFLT::HB0] = xFindPropertyOffset("f_dc_2" );

  m_NumSphHarmCoeffs = c_MaxNumSphHarm;
  for(int32 i = 0; i < c_MaxNumSphHarm; i++)
  {
    std::string PropertyName = fmt::format("f_rest_{}", i);
    int32       AttrOffset   = xFindPropertyOffset(PropertyName);
    if(AttrOffset < 0) { m_NumSphHarmCoeffs = i - 1; break; }
  }
  m_NumSphHarm = m_NumSphHarmCoeffs / 3;

  for(int32 i = 0; i < m_NumSphHarm; i++)
  {
    xAttrFLT          AttrIdR = xAttrFLT(int32(xAttrFLT::HR1) + 3 * i);
    xAttrFLT          AttrIdG = xAttrFLT(int32(xAttrFLT::HG1) + 3 * i);
    xAttrFLT          AttrIdB = xAttrFLT(int32(xAttrFLT::HB1) + 3 * i);
    int32 PropertySuffixNumR  = c_SphHarmOrderPCO ? i                    : i * 3 + 0;
    int32 PropertySuffixNumG  = c_SphHarmOrderPCO ? i +     m_NumSphHarm : i * 3 + 1;
    int32 PropertySuffixNumB  = c_SphHarmOrderPCO ? i + 2 * m_NumSphHarm : i * 3 + 2;
    std::string PropertyNameR = fmt::format("f_rest_{}", PropertySuffixNumR);
    std::string PropertyNameG = fmt::format("f_rest_{}", PropertySuffixNumG);
    std::string PropertyNameB = fmt::format("f_rest_{}", PropertySuffixNumB);
    const int32 AttrOffsetR   = xFindPropertyOffset(PropertyNameR);
    const int32 AttrOffsetG   = xFindPropertyOffset(PropertyNameG);
    const int32 AttrOffsetB   = xFindPropertyOffset(PropertyNameB);
    m_AttrIdToOffset[AttrIdR] = AttrOffsetR;
    m_AttrIdToOffset[AttrIdG] = AttrOffsetG;
    m_AttrIdToOffset[AttrIdB] = AttrOffsetB;
  }

  assert(m_AttrIdToOffset.size() == (size_t)xAttrFLT::COUNT);

  return eRetv::Success;
}
std::string xPly2::xFormatHeaderGSC(int32 NumSplats, int32 NumSphericalHarmonics)
{
  std::string Header; Header.reserve(xMemory::getBestEffortSizePageBase());

  Header += "ply\n";
  Header += "format binary_little_endian 1.0\n";
  //Header += "comment Generated by PMBB\n";
  Header += "element vertex " + std::to_string(NumSplats) + "\n";
  Header += "property float x\n";
  Header += "property float y\n";
  Header += "property float z\n";
  //Header += "property float nx\n";
  //Header += "property float ny\n";
  //Header += "property float nz\n";
  Header += "property float f_dc_0\n";
  Header += "property float f_dc_1\n";
  Header += "property float f_dc_2\n";
  for(int32 i = 0; i < NumSphericalHarmonics * 3; i++) { Header += fmt::format("property float f_rest_{}\n", i); }
  Header += "property float opacity\n";
  Header += "property float scale_0\n";
  Header += "property float scale_1\n";
  Header += "property float scale_2\n";
  Header += "property float rot_0\n";
  Header += "property float rot_1\n";
  Header += "property float rot_2\n";
  Header += "property float rot_3\n";
  Header += "end_header\n";
  
  return Header;
}
std::string xPly2::xFormatHeaderPC(int32 NumUnits, bool Binary)
{
  std::string Header; Header.reserve(xMemory::getBestEffortSizePageBase());
  Header += "ply\n";
  if(Binary) { Header += "format binary_little_endian 1.0\n"; }
  else       { Header += "format ascii 1.0\n"; }
  Header += "comment Generated by PMBB\n";
  Header += "element vertex " + std::to_string(NumUnits) + "\n";
  Header += "property float x\n";
  Header += "property float y\n";
  Header += "property float z\n";
  Header += "property float red\n";
  Header += "property float green\n";
  Header += "property float blue\n";
  Header += "end_header\n";
  return Header;
}

//===============================================================================================================================================================================================================

xSeqPlyList::tResult xSeqPlyList::readGC(xGaussianCloud* DstCloud)
{
  if(m_OpMode != eMode::Read) { return { eRetv::Error, "OpMode does not allow Read" }; }
  if(m_OpMode == eMode::Read && m_CurrFrameIdx >= m_NumOfFrames) { return eRetv::EndOfFile; }
  if(m_SingleFile && m_CurrFrameIdx > 0) { return { eRetv::Error, fmt::format("Attempt to read multiple times from single file File={}", m_FileNamePattern) }; }
  
  const std::string& FrameFileName = xFormatFileName(m_CurrFrameIdx + m_1stFileIdx);

  //read cloud
  xStream File(FrameFileName, xStream::eMode::Read);
  if(!File.isValid()) { return eRetv::InexistentFile; }
  tResult Result = m_Ply.readGC(DstCloud, &File, false);
  if(!Result) { return Result; }
  File.closeFile();

  //set POC & update state
  DstCloud->setPOC(m_CurrFrameIdx);
  m_CurrFrameIdx += 1;

  return eRetv::Success;
}
xSeqPlyList::tResult xSeqPlyList::seekFrame(int32 FrameIdx)
{
  if(FrameIdx < 0 || (m_OpMode == eMode::Read && FrameIdx >= m_NumOfFrames)) { return eRetv::Error; }
  if(m_OpMode != eMode::Read) { return eRetv::Error; }

  m_CurrFrameIdx = FrameIdx;
  return eRetv::Success;
}
xSeqPlyList::tResult xSeqPlyList::xBackendOpen(tCSR FileNamePattern, eMode OpMode)
{
  m_FileNamePattern = FileNamePattern;

  eFmtSts FormatStatus = analyzeFormatString(FileNamePattern);
  if(FormatStatus == eFmtSts::Malformed) { return { eRetv::WrongArg, fmt::format("Malformed format string FileNamePattern={}", FileNamePattern) }; }

  m_SingleFile = FormatStatus==eFmtSts::NonFmt; //file name pattern does not contain any format field

  switch(OpMode)
  {
  case eMode::Read : return xPlyListOpenRead (); break;
  case eMode::Write: return xPlyListOpenWrite(); break;
  default: return eRetv::NotImplemented; break;
  }
}
xSeqPlyList::tResult xSeqPlyList::xBackendClose()
{
  m_OpMode = eMode::Unknown;

  m_NumOfFrames  = NOT_VALID;
  m_CurrFrameIdx = NOT_VALID;

  return eRetv::Success;
}
xSeqPlyList::tResult xSeqPlyList::xPlyListOpenRead()
{
  if(m_SingleFile) //file does not contain any format field
  {
    m_NumOfFrames  = 1;
    m_CurrFrameIdx = 0;
    return xPlyListFileVerify(m_FileNamePattern);
  }  

  xFileListUtils::tIntRes FirstFrameRes = xFileListUtils::detectFirstFrame(m_FileNamePattern, std::bind(&xSeqPlyList::xPlyListFileVerify, this, std::placeholders::_1));
  if(!FirstFrameRes.second) { return FirstFrameRes.second; }
  m_1stFileIdx = FirstFrameRes.first;

  xFileListUtils::tIntRes NumFramesRes = xFileListUtils::detectNumFrames(m_FileNamePattern, m_1stFileIdx, m_MaxNumFiles);
  if(!NumFramesRes.second) { return NumFramesRes.second; }
  m_NumOfFrames  = NumFramesRes.first;
  m_CurrFrameIdx = 0;

  return eRetv::Success;
}
xSeqPlyList::tResult xSeqPlyList::xPlyListOpenWrite()
{
  m_1stFileIdx   = 0;
  m_NumOfFrames  = 0;
  m_CurrFrameIdx = 0;

  return eRetv::Success;
}
xSeqPlyList::tResult xSeqPlyList::xPlyListFileVerify(tCSR FileName)
{
  //Open file
  xStream File(FileName, xStream::eMode::Read);
  if(!File.isValid()) { return eRetv::InexistentFile; }

  xGaussianCloud Cloud;

  tResult PlyHeaderReadResult = m_Ply.readGC(&Cloud, &File, true);

  File.closeFile();

  return PlyHeaderReadResult;
}

//===============================================================================================================================================================================================================

void xPly::reset()
{ 
  m_NumSplats        = NOT_VALID;
  m_NumSphHarmCoeffs = NOT_VALID;
  m_NumSphHarm       = NOT_VALID;  
  m_PropertyOffset.clear();
  m_AttrIdToOffset.clear();
}
void xPly::destroy()
{
  m_OpMode = eMode::Unknown;
  m_Stream = nullptr;
}
xSeqFile::tResult xPly::xBackendOpen(tCSR FileName, eMode OpMode)
{
  m_Stream = new xStream();
  switch(OpMode)
  {
    case eMode::Read  : m_Stream->openFile(FileName, xStream::eMode::Read  ); break;
    case eMode::Write : m_Stream->openFile(FileName, xStream::eMode::Write ); break;
    default: return eRetv::WrongArg;
  }

  if(!m_Stream->isValid()) { return eRetv::Error; }

  return eRetv::Success;
}
xSeq::tResult xPly::xBackendClose()
{
  m_Stream->closeFile(); delete(m_Stream); m_Stream = nullptr;
  m_OpMode = eMode::Unknown;

  return eRetv::Success;
}
xSeqFile::tResult xPly::readGC(xGaussianCloud* DstCloud, bool HeaderOnly)
{
  if (m_OpMode != eMode::Read) { return { eRetv::Error, "OpMode does not allow Read" }; }

  m_CameraData.clear();
  m_NumSplats        = NOT_VALID;
  m_NumSphHarmCoeffs = 0;
  m_NumSphHarm       = 0;
  m_PropertyOffset.clear();
  m_AttrIdToOffset.clear();

  std::string Header = xFindHeader();
  if(Header.empty()) { return { eRetv::Error, "PLY header cannot be found !" }; }

  tResult HeaderRes = xParseHeaderGSC(Header);
  if(!HeaderRes) { return HeaderRes; }

  const int32 SphHarmDegree = xSplatOps::xSphHarmDegreeFromCount(m_NumSphHarm);
  const int32 NumProperties = (int32)m_PropertyOffset.size();

  DstCloud->create          (m_NumSplats);
  DstCloud->setCameraData   (m_CameraData);
  DstCloud->setSphHarmDegree(SphHarmDegree);
  DstCloud->setSphHarmCount (m_NumSphHarm );
  DstCloud->setAttCount     ((uint32)m_PropertyOffset.size());
  
  if(HeaderOnly) { return eRetv::Success; }

  //read cloud
  std::vector<flt32> TmpBuff(c_NumSplatsInBatch * NumProperties, std::numeric_limits<flt32>::quiet_NaN());

  for(int32 BatchSplatIdx = 0; BatchSplatIdx < m_NumSplats; BatchSplatIdx += c_NumSplatsInBatch)
  {
    int32 NumSplatsInCurrBatch = xMin(c_NumSplatsInBatch, m_NumSplats - BatchSplatIdx);
    bool ReadR = m_Stream->read(TmpBuff.data(), NumSplatsInCurrBatch * NumProperties * sizeof(flt32));
    if(!ReadR) { return { eRetv::Error, "PLY cloud read error" }; }
    
    for(uint32 AttrIdx = 0; AttrIdx < (uint32)xAttrFLT::COUNT; AttrIdx++)
    {
      const xAttrFLT  AttrId     = (xAttrFLT)AttrIdx;
      const uint32    AttrOffset = m_AttrIdToOffset[AttrId];
      const flt32*    SrcPtr     = TmpBuff.data() + AttrOffset;
      flt32* restrict DstPtr     = DstCloud->getAttrAddr(AttrId);
      for(int32 SrcSplatIdx = 0, DstSplatIdx = BatchSplatIdx; SrcSplatIdx < NumSplatsInCurrBatch; SrcSplatIdx++, DstSplatIdx++)
      {        
        flt32 AttrVal = SrcPtr[SrcSplatIdx * NumProperties];
        DstPtr[DstSplatIdx] = AttrVal;
      }
    }
  }

  return eRetv::Success;
}

xSeqFile::tResult xPly::writeGC(const xGaussianCloud* SrcCloud)
{
  const int32 NumSplats     = SrcCloud->getNumSplats();
  const int32 NumSphHarm    = xSplatOps::xSphHarmCountFromDegree(SrcCloud->getSphHarmDegree());
  const int32 NumProperties = 14 + NumSphHarm * 3;

  std::string Header = xFormatHeaderGSC(NumSplats, NumSphHarm);
  //fmt::print(Header);
  m_Stream->write(Header);

  //a bit dirty trick, but saves a lot of effort with building offset tables
  xParseHeaderGSC(Header);

  assert(m_NumSplats                    == NumSplats    );
  assert((int32)m_PropertyOffset.size() == NumProperties);
  assert(m_NumSphHarm                   == NumSphHarm   );

  std::vector<flt32> TmpBuff(c_NumSplatsInBatch * NumProperties, std::numeric_limits<flt32>::quiet_NaN());

  for(int32 BatchSplatIdx = 0; BatchSplatIdx < m_NumSplats; BatchSplatIdx += c_NumSplatsInBatch)
  {
    int32 NumSplatsInCurrBatch = xMin(c_NumSplatsInBatch, m_NumSplats - BatchSplatIdx);

    for(uint32 AttrIdx = 0; AttrIdx < (uint32)xAttrFLT::COUNT; AttrIdx++)
    {
      const xAttrFLT  AttrId     = (xAttrFLT)AttrIdx;
      const uint32    AttrOffset = m_AttrIdToOffset[AttrId];
      const flt32*    SrcPtr     = SrcCloud->getAttrAddr(AttrId);
      flt32* restrict DstPtr     = TmpBuff.data() + AttrOffset;
      for(int32 DstSplatIdx = 0, SrcSplatIdx = BatchSplatIdx; DstSplatIdx < NumSplatsInCurrBatch; DstSplatIdx++, SrcSplatIdx++)
      {
        flt32 AttrVal = SrcPtr[SrcSplatIdx];
        DstPtr[DstSplatIdx * NumProperties] = AttrVal;
      }
    }

    ////dirty hack for unused nx, ny, and nz
    //for(uint32 FakeOffset = 3; FakeOffset < 6; FakeOffset++)
    //{
    //  flt32* restrict DstPtr = TmpBuff.data() + FakeOffset;
    //  for(int32 DstSplatIdx = 0, SrcSplatIdx = BatchSplatIdx; DstSplatIdx < NumSplatsInCurrBatch; DstSplatIdx++, SrcSplatIdx++)
    //  {
    //    DstPtr[DstSplatIdx * NumProperties] = 0.0f;
    //  }
    //}

    m_Stream->write(TmpBuff.data(), NumSplatsInCurrBatch * NumProperties * sizeof(flt32));
  }

  return eRetv::Success;
}

xSeqFile::tResult xPly::writePC(const xPointCloud* SrcCloud, bool Binary)
{
  const int32 NumPoints = SrcCloud->getNumValidPoints();

  const xPointCloud::tCoordEl* CoordsPtr = SrcCloud->getCoordsPtrRaw();
  const xPointCloud::tColorEl* ColorsPtr = SrcCloud->getColorsPtrRaw();

  std::string Header = xFormatHeaderPC(NumPoints, Binary);
  m_Stream->write(Header);

  if(Binary)
  {
    const int32 NumProperties = 6;
    std::vector<flt32> TmpBuff(c_NumPointsInBatch * NumProperties, std::numeric_limits<flt32>::quiet_NaN());

    flt32* restrict DstPtr = TmpBuff.data();

    for(int32 BatchPointIdx = 0; BatchPointIdx < NumPoints; BatchPointIdx += c_NumPointsInBatch)
    {
      int32 NumPointsInCurrBatch = xMin(c_NumPointsInBatch, NumPoints - BatchPointIdx);
      for(int32 DstPointIdx = 0, SrcPointIdx = BatchPointIdx; DstPointIdx < NumPointsInCurrBatch; DstPointIdx++, SrcPointIdx++)
      {
        DstPtr[DstPointIdx * NumProperties + 0] = CoordsPtr[(SrcPointIdx << 2) + 0];
        DstPtr[DstPointIdx * NumProperties + 1] = CoordsPtr[(SrcPointIdx << 2) + 1];
        DstPtr[DstPointIdx * NumProperties + 2] = CoordsPtr[(SrcPointIdx << 2) + 2];
        DstPtr[DstPointIdx * NumProperties + 3] = ColorsPtr[(SrcPointIdx << 2) + 0]  * 255;
        DstPtr[DstPointIdx * NumProperties + 4] = ColorsPtr[(SrcPointIdx << 2) + 1]  * 255;
        DstPtr[DstPointIdx * NumProperties + 5] = ColorsPtr[(SrcPointIdx << 2) + 2]  * 255;
      }
      m_Stream->write(TmpBuff.data(), NumPointsInCurrBatch * NumProperties * sizeof(flt32));
    }
  }
  else
  {
    std::string Buffer; Buffer.reserve(xMax(xMemory::getBestEffortSizePageBase() * 64, xMemory::getRealSizePageHuge()));
    for(int32 i = 0; i < NumPoints; i++)
    {
      fmt::format_to(std::back_inserter(Buffer), "{0:12f} {1:12f} {2:12f} {3:12f} {4:12f} {5:12f}\n",
        CoordsPtr[(i << 2) + 0],
        CoordsPtr[(i << 2) + 1],
        CoordsPtr[(i << 2) + 2],
        ColorsPtr[(i << 2) + 0],
        ColorsPtr[(i << 2) + 1],
        ColorsPtr[(i << 2) + 2]);

      if(Buffer.size() > Buffer.capacity() - xMemory::getBestEffortSizePageBase())
      {
        m_Stream->write(Buffer);
        Buffer.clear();
      }
    }
  }

  return eRetv::Success;
}
std::string xPly::xFindHeader()
{
  static constexpr int64 ReadSize = 4096*16;
  const int64 FileSize = m_Stream->size();
  std::string TmpBuffer; TmpBuffer.resize(ReadSize);
  int64 NumRead1st = xMin((int64)TmpBuffer.size(), FileSize);
  m_Stream->read(TmpBuffer.data(), NumRead1st);
  int64 EndPos = TmpBuffer.find("end_header");;
  while(EndPos == NOT_VALID)
  {
    int64 CurrSize = TmpBuffer.size();
    TmpBuffer.resize(CurrSize + ReadSize);
    int64 NumReadNth = xMin((int64)TmpBuffer.size(), FileSize) - CurrSize;
    if(NumReadNth == 0) { return ""; };
    m_Stream->read(TmpBuffer.data(), NumRead1st);
    EndPos = TmpBuffer.find("end_header");;
  }
  EndPos = EndPos + 11;
  if(TmpBuffer[EndPos] == '\r' && TmpBuffer[EndPos+1] == '\n') //CRLF file
  {
    EndPos++;
  }
  TmpBuffer.resize(EndPos);
  //fmt::print(TmpBuffer);
  m_Stream->seekR(EndPos, xStream::eSeek::Beg);
  return TmpBuffer;
}
int32 xPly::xFindPropertyOffset(const std::string& AttributeName)
{
  const auto& Itr = m_PropertyOffset.find(AttributeName);
  if(Itr == m_PropertyOffset.end()) { return NOT_VALID; }
  return Itr->second;
}
xSeqFile::tResult xPly::xParseHeaderGSC(const std::string& Header)
{
  std::stringstream HeaderStream;
  HeaderStream.str(Header);

  std::string Line;

  //1st line have to contain "ply"
  std::getline(HeaderStream, Line);
  if(Line != "ply") { return { eRetv::Error, "not a .ply file" }; }

  //try next line
  std::istream &Result = std::getline(HeaderStream, Line);
  if(!Result) { return { eRetv::Error, "missing header data" }; }

  //try parse comments
  while(Line.find("comment") == 0)
  {
    if(Line.find("camera_position_header") != std::string::npos) //camera position header
    {
      std::vector<std::string> HeadLineSplit = xString::split(Line, ' ');
      //TODO read this line to get information about location of each parameter

      std::getline(HeaderStream, Line);
      while(Line.find("camera_position") != std::string::npos)
      {
        std::vector<std::string> LineSplit = xString::split(Line, ' ');

        int32 ImgIdx = std::stoi(LineSplit[ 2]);
        int32 CamIdx = std::stoi(LineSplit[ 3]);
        flt64 PosX   = std::stod(LineSplit[ 4]);
        flt64 PosY   = std::stod(LineSplit[ 5]);
        flt64 PosZ   = std::stod(LineSplit[ 6]);
        flt64 QuatW  = std::stod(LineSplit[ 7]);
        flt64 QuatX  = std::stod(LineSplit[ 8]);
        flt64 QuatY  = std::stod(LineSplit[ 9]);
        flt64 QuatZ  = std::stod(LineSplit[10]);
        flt64 FocalX = std::stod(LineSplit[11]);
        flt64 FocalY = std::stod(LineSplit[12]);
        tStr  Name   = LineSplit[13];

        m_CameraData.emplace_back(ImgIdx, CamIdx, flt64V3{ PosX , PosY, PosZ }, flt64V4{ QuatW, QuatX, QuatY, QuatZ, }, flt64V2{ FocalX, FocalY }, Name);

        std::getline(HeaderStream, Line);
      }
    }
    else
    {
      return { eRetv::Error, "unknown comment" };
    }
    if(Line.find("comment") == 0) { std::getline(HeaderStream, Line); }
  }

  if(Line != "format binary_little_endian 1.0") { return { eRetv::Error, "only binary_little_endian 1.0 format is accepted" }; }

  std::getline(HeaderStream, Line);
  if(Line.find("element vertex ") != 0) { return { eRetv::Error, "missing vertex count" }; }

  m_NumSplats = std::stoi(Line.substr(std::strlen("element vertex ")));

  if(m_NumSplats <= 0) { return { eRetv::Error, fmt::format("invalid vertex count: {}", m_NumSplats) }; }

  // Parse attribute names
  for(int32 i = 0; ; i++)
  {
    std::getline(HeaderStream, Line);
    if(Line == "end_header") { break; }
    if(Line.substr(0, c_PropertyPatternLen) != c_PropertyPattern) { return { eRetv::Error, fmt::format("unsupported property: [{}]", Line) }; }
    std::string AttrStr = Line.substr(c_PropertyPatternLen);
    m_PropertyOffset[AttrStr] = i;
  }

  m_AttrIdToOffset[xAttrFLT::PX ] = xFindPropertyOffset("x"      );
  m_AttrIdToOffset[xAttrFLT::PY ] = xFindPropertyOffset("y"      );
  m_AttrIdToOffset[xAttrFLT::PZ ] = xFindPropertyOffset("z"      );
  m_AttrIdToOffset[xAttrFLT::SX ] = xFindPropertyOffset("scale_0");
  m_AttrIdToOffset[xAttrFLT::SY ] = xFindPropertyOffset("scale_1");
  m_AttrIdToOffset[xAttrFLT::SZ ] = xFindPropertyOffset("scale_2");
  m_AttrIdToOffset[xAttrFLT::RX ] = xFindPropertyOffset("rot_1"  );
  m_AttrIdToOffset[xAttrFLT::RY ] = xFindPropertyOffset("rot_2"  );
  m_AttrIdToOffset[xAttrFLT::RZ ] = xFindPropertyOffset("rot_3"  );
  m_AttrIdToOffset[xAttrFLT::RW ] = xFindPropertyOffset("rot_0"  );
  m_AttrIdToOffset[xAttrFLT::OP ] = xFindPropertyOffset("opacity");
  m_AttrIdToOffset[xAttrFLT::HR0] = xFindPropertyOffset("f_dc_0" );
  m_AttrIdToOffset[xAttrFLT::HG0] = xFindPropertyOffset("f_dc_1" );
  m_AttrIdToOffset[xAttrFLT::HB0] = xFindPropertyOffset("f_dc_2" );

  m_NumSphHarmCoeffs = c_MaxNumSphHarm;
  for(int32 i = 0; i < c_MaxNumSphHarm; i++)
  {
    std::string PropertyName = fmt::format("f_rest_{}", i);
    int32       AttrOffset   = xFindPropertyOffset(PropertyName);
    if(AttrOffset < 0) { m_NumSphHarmCoeffs = i - 1; break; }
  }
  m_NumSphHarm = m_NumSphHarmCoeffs / 3;

  for(int32 i = 0; i < m_NumSphHarm; i++)
  {
    xAttrFLT          AttrIdR = xAttrFLT(int32(xAttrFLT::HR1) + 3 * i);
    xAttrFLT          AttrIdG = xAttrFLT(int32(xAttrFLT::HG1) + 3 * i);
    xAttrFLT          AttrIdB = xAttrFLT(int32(xAttrFLT::HB1) + 3 * i);
    int32 PropertySuffixNumR  = c_SphHarmOrderPCO ? i                    : i * 3 + 0;
    int32 PropertySuffixNumG  = c_SphHarmOrderPCO ? i +     m_NumSphHarm : i * 3 + 1;
    int32 PropertySuffixNumB  = c_SphHarmOrderPCO ? i + 2 * m_NumSphHarm : i * 3 + 2;
    std::string PropertyNameR = fmt::format("f_rest_{}", PropertySuffixNumR);
    std::string PropertyNameG = fmt::format("f_rest_{}", PropertySuffixNumG);
    std::string PropertyNameB = fmt::format("f_rest_{}", PropertySuffixNumB);
    const int32 AttrOffsetR   = xFindPropertyOffset(PropertyNameR);
    const int32 AttrOffsetG   = xFindPropertyOffset(PropertyNameG);
    const int32 AttrOffsetB   = xFindPropertyOffset(PropertyNameB);
    m_AttrIdToOffset[AttrIdR] = AttrOffsetR;
    m_AttrIdToOffset[AttrIdG] = AttrOffsetG;
    m_AttrIdToOffset[AttrIdB] = AttrOffsetB;
  }

  assert(m_AttrIdToOffset.size() == (size_t)xAttrFLT::COUNT);

  return eRetv::Success;
}
std::string xPly::xFormatHeaderGSC(int32 NumSplats, int32 NumSphericalHarmonics)
{
  std::string Header; Header.reserve(xMemory::getBestEffortSizePageBase());

  Header += "ply\n";
  Header += "format binary_little_endian 1.0\n";
  //Header += "comment Generated by PMBB\n";
  Header += "element vertex " + std::to_string(NumSplats) + "\n";
  Header += "property float x\n";
  Header += "property float y\n";
  Header += "property float z\n";
  //Header += "property float nx\n";
  //Header += "property float ny\n";
  //Header += "property float nz\n";
  Header += "property float f_dc_0\n";
  Header += "property float f_dc_1\n";
  Header += "property float f_dc_2\n";
  for(int32 i = 0; i < NumSphericalHarmonics * 3; i++) { Header += fmt::format("property float f_rest_{}\n", i); }
  Header += "property float opacity\n";
  Header += "property float scale_0\n";
  Header += "property float scale_1\n";
  Header += "property float scale_2\n";
  Header += "property float rot_0\n";
  Header += "property float rot_1\n";
  Header += "property float rot_2\n";
  Header += "property float rot_3\n";
  Header += "end_header\n";
  
  return Header;
}
std::string xPly::xFormatHeaderPC(int32 NumUnits, bool Binary)
{
  std::string Header; Header.reserve(xMemory::getBestEffortSizePageBase());
  Header += "ply\n";
  if(Binary) { Header += "format binary_little_endian 1.0\n"; }
  else       { Header += "format ascii 1.0\n"; }
  Header += "comment Generated by PMBB\n";
  Header += "element vertex " + std::to_string(NumUnits) + "\n";
  Header += "property float x\n";
  Header += "property float y\n";
  Header += "property float z\n";
  Header += "property float red\n";
  Header += "property float green\n";
  Header += "property float blue\n";
  Header += "end_header\n";
  return Header;
}

//===============================================================================================================================================================================================================

} //end of namespace PMBB