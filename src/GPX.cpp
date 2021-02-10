#include "GPX.h"

GPX::GPX(){
}

String GPX::getOpen(){
  return String(_GPX_HEAD);
}

String GPX::getClose(){
  return String(_GPX_TAIL);
}

String GPX::getMetaData(){
  String localStr(_GPX_META_HEAD);
  if (_metaName.length() > 0){
    localStr = localStr + String(_GPX_NAME_HEAD);
    localStr = localStr + _metaName;
    localStr = localStr + String(_GPX_NAME_TAIL);
  }
  if (_metaDesc.length() > 0){
    localStr = localStr + String(_GPX_DESC_HEAD);
    localStr = localStr + _metaDesc;
    localStr = localStr + String(_GPX_DESC_TAIL);
  }
  localStr = localStr + String(_GPX_META_TAIL);
  return localStr;
}

String GPX::getTrakOpen(){
  return String(_GPX_TRAK_HEAD);
}

String GPX::getTrakClose(){
  return String(_GPX_TRAK_TAIL);
}

String GPX::getTrakSegOpen(){
  return String(_GPX_TRKSEG_HEAD);
}

String GPX::getTrakSegClose(){
  return String(_GPX_TRKSEG_TAIL);
}

String GPX::getInfo(){
  String localStr("");
  if (_name.length() > 0){
    localStr += _GPX_NAME_HEAD;
    localStr += _name;
    localStr += _GPX_NAME_TAIL;
  }
  if (_desc.length() > 0){
    localStr += _GPX_DESC_HEAD;
    localStr += _desc;
    localStr += _GPX_DESC_TAIL;
  }
  return localStr;
}

String GPX::getTrackPt(float lat, float lon, float ele, float speed, String time)
{
    char line[256];
    sprintf(line, "<trkpt lat=\"%4.6f\" lon=\"%4.6f\"><ele>%5.1f</ele><speed>%3.1f</speed><time>%s</time></trkpt>\n", lat, lon, ele, speed, time.c_str());
    return String(line);
}

void GPX::setMetaName(String name){
  _metaName = name;
}
void GPX::setMetaDesc(String desc){
  _metaDesc = desc;
}
void GPX::setName(String name){
  _name = name;
}
void GPX::setDesc(String desc){
  _desc = desc;
}
void GPX::setEle(String ele){
  _ele = ele;
}
void GPX::setSym(String sym){
  _sym = sym;
}
void GPX::setSrc(String src){
  _src = src;
}
