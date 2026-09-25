#pragma once
#include <Geode/utils/ZStringView.hpp>
#include <Geode/cocos/cocoa/CCObject.h>
#include <Geode/cocos/base_nodes/CCNode.h>
#include <Geode/cocos/include/CCProtocols.h>
#include <Geode/utils/cocos.hpp>
#include <Geode/utils/casts.hpp>
namespace alpha::utils::cocos {
static inline bool setColorByHex(cocos2d::CCRGBAProtocol* node,geode::ZStringView colorHex){auto color=geode::cocos::cc3bFromHexString(colorHex);if(color.isOk()){node->setColor(color.unwrap());return true;}return false;}
}