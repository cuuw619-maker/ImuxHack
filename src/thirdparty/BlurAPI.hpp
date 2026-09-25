#pragma once
#include <Geode/Geode.hpp>
#define BLUR_TAG "thesillydoggo.blur-api/blur-options"
namespace BlurAPI {
class BlurOptions:public cocos2d::CCObject{public:int apiVersion=1;cocos2d::CCRenderTexture* rTex=nullptr;geode::Ref<cocos2d::CCClippingNode> clip=nullptr;bool forcePasses=false;int passes=3;float alphaThreshold=.01f;virtual bool init(){return true;}CREATE_FUNC(BlurOptions);};
inline BlurOptions* getOptions(cocos2d::CCNode* node){return static_cast<BlurOptions*>(node->getUserObject(BLUR_TAG));}
inline void addBlur(cocos2d::CCNode* node){if(!getOptions(node))node->setUserObject(BLUR_TAG,BlurOptions::create());}
inline void removeBlur(cocos2d::CCNode* node){node->setUserObject(BLUR_TAG,nullptr);}
inline bool isBlurAPIEnabled(){if(auto blur=geode::Loader::get()->getLoadedMod("thesillydoggo.blur-api"))return blur->getSettingValue<bool>("enabled");return false;}
inline bool willLoad(){if(auto blur=geode::Loader::get()->getInstalledMod("thesillydoggo.blur-api"))return blur->shouldLoad();return false;}
}