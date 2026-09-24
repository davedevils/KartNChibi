// the old room nifs carry their textures inside as NiPixelData written out as dds for the renderer
#pragma once

#include <map>
#include <string>

namespace KnC::Render { struct MapScene; struct PropModel; }

namespace KnC::Client {

// lower case file name of each embedded texture to the dds written under the cache folder empty when none
std::map<std::string, std::string> extractEmbeddedTextures(const std::string& nifPath, const std::string& cacheDir);

// points every part and particle system at the dds of its embedded texture when the named file is missing
void applyEmbeddedTextures(const std::map<std::string, std::string>& embedded, KnC::Render::PropModel& model);
void applyEmbeddedTextures(const std::map<std::string, std::string>& embedded, KnC::Render::MapScene& scene);

}
