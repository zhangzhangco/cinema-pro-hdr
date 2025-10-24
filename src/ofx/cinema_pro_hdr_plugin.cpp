#include "ofxsImageEffect.h"
#include "ofxsMultiThread.h"
#include "ofxsParam.h"

#include "cinema_pro_hdr/core.h"
#include "cinema_pro_hdr/processor.h"

#include <string>
#include <iostream>

// Plugin identifiers
#define kPluginName "CinemaProHDR"
#define kPluginGrouping "Color"
#define kPluginDescription "Cinema Pro HDR processing plugin for professional color grading"
#define kPluginIdentifier "com.cinemapro.hdr"
#define kPluginVersionMajor 1
#define kPluginVersionMinor 0

// Parameter names
#define kParamTargetNits "targetNits"
#define kParamSourceColorSpace "sourceColorSpace"
#define kParamTargetColorSpace "targetColorSpace"
#define kParamEnableStatistics "enableStatistics"

// Plugin class
class CinemaProHDRPlugin : public OFX::ImageEffect {
private:
    OFX::Clip* srcClip_;
    OFX::Clip* dstClip_;
    
    OFX::DoubleParam* targetNitsParam_;
    OFX::ChoiceParam* sourceColorSpaceParam_;
    OFX::ChoiceParam* targetColorSpaceParam_;
    OFX::BooleanParam* enableStatisticsParam_;

public:
    CinemaProHDRPlugin(OfxImageEffectHandle handle);
    
    virtual void render(const OFX::RenderArguments &args) override;
    virtual bool isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip * &identityClip, double &identityTime, int& view, std::string& plane) override;
    virtual void changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) override;
};

CinemaProHDRPlugin::CinemaProHDRPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , srcClip_(nullptr)
    , dstClip_(nullptr)
    , targetNitsParam_(nullptr)
    , sourceColorSpaceParam_(nullptr)
    , targetColorSpaceParam_(nullptr)
    , enableStatisticsParam_(nullptr)
{
    srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
    dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
    
    targetNitsParam_ = fetchDoubleParam(kParamTargetNits);
    sourceColorSpaceParam_ = fetchChoiceParam(kParamSourceColorSpace);
    targetColorSpaceParam_ = fetchChoiceParam(kParamTargetColorSpace);
    enableStatisticsParam_ = fetchBooleanParam(kParamEnableStatistics);
}

void CinemaProHDRPlugin::render(const OFX::RenderArguments &args) {
    if (!srcClip_ || !dstClip_) return;
    
    // Get source image
    std::unique_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));
    if (!src.get()) {
        throwSuiteStatusException(kOfxStatFailed);
        return;
    }
    
    // Get destination image
    std::unique_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));
    if (!dst.get()) {
        throwSuiteStatusException(kOfxStatFailed);
        return;
    }
    
    // Get parameters
    double targetNits = targetNitsParam_->getValueAtTime(args.time);
    int sourceColorSpace = sourceColorSpaceParam_->getValueAtTime(args.time);
    int targetColorSpace = targetColorSpaceParam_->getValueAtTime(args.time);
    bool enableStats = enableStatisticsParam_->getValueAtTime(args.time);
    
    // Get image properties
    OfxRectI renderWindow = args.renderWindow;
    int width = renderWindow.x2 - renderWindow.x1;
    int height = renderWindow.y2 - renderWindow.y1;
    
    // Create Cinema Pro HDR parameters
    CinemaProHDR::CPHParams params;
    params.target_nits = targetNits;
    params.source_color_space = static_cast<CinemaProHDR::ColorSpace>(sourceColorSpace);
    params.target_color_space = static_cast<CinemaProHDR::ColorSpace>(targetColorSpace);
    params.enable_statistics = enableStats;
    
    // Create processor
    CinemaProHDR::CPHProcessor processor(params);
    
    // Process image data
    const void* srcData = src->getPixelData();
    void* dstData = dst->getPixelData();
    
    if (srcData && dstData) {
        // Create image objects
        CinemaProHDR::Image srcImage(width, height, static_cast<const float*>(srcData));
        CinemaProHDR::Image dstImage(width, height, static_cast<float*>(dstData));
        
        // Process
        CinemaProHDR::ErrorReport error;
        if (!processor.process(srcImage, dstImage, error)) {
            std::cerr << "Cinema Pro HDR processing failed: " << error.message << std::endl;
        }
    }
}

bool CinemaProHDRPlugin::isIdentity(const OFX::IsIdentityArguments &args, OFX::Clip * &identityClip, double &identityTime, int& view, std::string& plane) {
    // Check if processing would result in no change
    double targetNits = targetNitsParam_->getValueAtTime(args.time);
    int sourceColorSpace = sourceColorSpaceParam_->getValueAtTime(args.time);
    int targetColorSpace = targetColorSpaceParam_->getValueAtTime(args.time);
    
    // If source and target are the same and target nits is default, it's identity
    if (sourceColorSpace == targetColorSpace && targetNits == 100.0) {
        identityClip = srcClip_;
        identityTime = args.time;
        view = args.view;
        plane = args.plane;
        return true;
    }
    
    return false;
}

void CinemaProHDRPlugin::changedParam(const OFX::InstanceChangedArgs &args, const std::string &paramName) {
    // Handle parameter changes if needed
    if (paramName == kParamTargetNits || 
        paramName == kParamSourceColorSpace || 
        paramName == kParamTargetColorSpace) {
        // Invalidate cache when important parameters change
        clearPersistentMessage();
    }
}

// Plugin factory
class CinemaProHDRPluginFactory : public OFX::PluginFactoryHelper<CinemaProHDRPluginFactory> {
public:
    CinemaProHDRPluginFactory() : OFX::PluginFactoryHelper<CinemaProHDRPluginFactory>(kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor) {}
    
    virtual void describe(OFX::ImageEffectDescriptor &desc) override;
    virtual void describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) override;
    virtual OFX::ImageEffect* createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context) override;
};

void CinemaProHDRPluginFactory::describe(OFX::ImageEffectDescriptor &desc) {
    desc.setLabels(kPluginName, kPluginName, kPluginName);
    desc.setPluginGrouping(kPluginGrouping);
    desc.setPluginDescription(kPluginDescription);
    
    desc.addSupportedContext(eContextFilter);
    desc.addSupportedBitDepth(eBitDepthFloat);
    
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(true);
    desc.setSupportsTiles(true);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);
}

void CinemaProHDRPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context) {
    // Source clip
    OFX::ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);
    
    // Output clip
    OFX::ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->addSupportedComponent(ePixelComponentRGB);
    dstClip->setSupportsTiles(true);
    
    // Parameters
    OFX::PageParamDescriptor *page = desc.definePageParam("Controls");
    
    // Target nits parameter
    {
        OFX::DoubleParamDescriptor *param = desc.defineDoubleParam(kParamTargetNits);
        param->setLabels("Target Nits", "Target Nits", "Target Nits");
        param->setScriptName(kParamTargetNits);
        param->setHint("Target peak luminance in nits");
        param->setDefault(100.0);
        param->setRange(1.0, 10000.0);
        param->setDisplayRange(10.0, 4000.0);
        param->setDoubleType(eDoubleTypeScale);
        page->addChild(*param);
    }
    
    // Source color space parameter
    {
        OFX::ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamSourceColorSpace);
        param->setLabels("Source Color Space", "Source Color Space", "Source Color Space");
        param->setScriptName(kParamSourceColorSpace);
        param->setHint("Source image color space");
        param->appendOption("Rec709");
        param->appendOption("Rec2020");
        param->appendOption("P3");
        param->appendOption("ACES");
        param->setDefault(0);
        page->addChild(*param);
    }
    
    // Target color space parameter
    {
        OFX::ChoiceParamDescriptor *param = desc.defineChoiceParam(kParamTargetColorSpace);
        param->setLabels("Target Color Space", "Target Color Space", "Target Color Space");
        param->setScriptName(kParamTargetColorSpace);
        param->setHint("Target output color space");
        param->appendOption("Rec709");
        param->appendOption("Rec2020");
        param->appendOption("P3");
        param->appendOption("ACES");
        param->setDefault(1);
        page->addChild(*param);
    }
    
    // Enable statistics parameter
    {
        OFX::BooleanParamDescriptor *param = desc.defineBooleanParam(kParamEnableStatistics);
        param->setLabels("Enable Statistics", "Enable Statistics", "Enable Statistics");
        param->setScriptName(kParamEnableStatistics);
        param->setHint("Enable statistical analysis and reporting");
        param->setDefault(false);
        page->addChild(*param);
    }
}

OFX::ImageEffect* CinemaProHDRPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context) {
    return new CinemaProHDRPlugin(handle);
}

// Plugin registration
static CinemaProHDRPluginFactory p;
mDeclarePluginFactory(CinemaProHDRPluginFactory, p, {});