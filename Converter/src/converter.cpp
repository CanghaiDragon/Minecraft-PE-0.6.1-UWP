#include "converter.h"
#include "legacy_biome.h"
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <zlib.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace {
constexpr size_t Sector = 4096, Header = 4096, ChunkBytes = 32768 + 3 * 16384 + 256;
constexpr size_t BatchLimit = 8u * 1024u * 1024u;
constexpr size_t BlockBytes = 32768, NibbleBytes = 16384;
using Bytes = std::vector<uint8_t>;

uint16_t u16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t u32(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24; }
uint32_t u32be(const uint8_t* p) { return uint32_t(p[3]) | uint32_t(p[2]) << 8 | uint32_t(p[1]) << 16 | uint32_t(p[0]) << 24; }
void put32(Bytes& out, uint32_t v) { out.insert(out.end(), {uint8_t(v),uint8_t(v>>8),uint8_t(v>>16),uint8_t(v>>24)}); }
void put32be(Bytes& out, uint32_t v) { out.insert(out.end(), {uint8_t(v>>24),uint8_t(v>>16),uint8_t(v>>8),uint8_t(v)}); }
void put64(Bytes& out, uint64_t v) { for (int i=0;i<8;++i) out.push_back(uint8_t(v>>(i*8))); }
void put16(Bytes& out, uint16_t v) { out.push_back(uint8_t(v)); out.push_back(uint8_t(v>>8)); }
void need(bool v, const char* m) { if (!v) throw std::runtime_error(m); }

struct Tag {
 uint8_t type{}; std::string name; Bytes scalar; uint8_t listType{}; std::vector<Tag> children;
};
struct Reader { const Bytes& b; size_t p{}; uint8_t one(){need(p<b.size(),"NBT truncated");return b[p++];} uint16_t s(){need(p+2<=b.size(),"NBT truncated");auto v=u16(b.data()+p);p+=2;return v;} uint32_t i(){need(p+4<=b.size(),"NBT truncated");auto v=u32(b.data()+p);p+=4;return v;} Bytes take(size_t n){need(p+n<=b.size(),"NBT truncated");Bytes v(b.begin()+p,b.begin()+p+n);p+=n;return v;} };
Tag readPayload(Reader& r,uint8_t type);
Tag readNamed(Reader& r) { Tag t; t.type=r.one(); if(!t.type)return t; auto n=r.s(); auto x=r.take(n); t.name.assign(x.begin(),x.end()); Tag q=readPayload(r,t.type); q.type=t.type;q.name=t.name;return q; }
Tag readPayload(Reader& r,uint8_t type) { Tag t; t.type=type; size_t n=0; switch(type) {
 case 1:n=1;break; case 2:n=2;break; case 3:case 5:n=4;break; case 4:case 6:n=8;break;
 case 7:n=r.i();t.scalar=r.take(n);return t;
 case 8:n=r.s();t.scalar=r.take(n);return t;
 case 9:t.listType=r.one();n=r.i();for(size_t i=0;i<n;++i)t.children.push_back(readPayload(r,t.listType));return t;
 case 10:while(true){Tag c=readNamed(r);if(!c.type)break;t.children.push_back(std::move(c));}return t;
 case 11:n=r.i()*4;t.scalar=r.take(n);return t; default:throw std::runtime_error("unknown NBT tag"); }
 t.scalar=r.take(n);return t;
}
void writePayload(const Tag& t,Bytes& o) { switch(t.type) {
 case 1:case 2:case 3:case 4:case 5:case 6:o.insert(o.end(),t.scalar.begin(),t.scalar.end());break;
 case 7:put32(o,(uint32_t)t.scalar.size());o.insert(o.end(),t.scalar.begin(),t.scalar.end());break;
 case 8:put16(o,(uint16_t)t.scalar.size());o.insert(o.end(),t.scalar.begin(),t.scalar.end());break;
 case 9:o.push_back(t.listType);put32(o,(uint32_t)t.children.size());for(auto& c:t.children)writePayload(c,o);break;
 case 10:for(auto& c:t.children){o.push_back(c.type);put16(o,(uint16_t)c.name.size());o.insert(o.end(),c.name.begin(),c.name.end());writePayload(c,o);}o.push_back(0);break;
 case 11:put32(o,(uint32_t)(t.scalar.size()/4));o.insert(o.end(),t.scalar.begin(),t.scalar.end());break; }
}
Bytes writeRoot(const Tag& root) { Bytes o; o.push_back(root.type);put16(o,(uint16_t)root.name.size());o.insert(o.end(),root.name.begin(),root.name.end());writePayload(root,o);return o; }
Tag child(const Tag& root,const char* name) { for(auto& c:root.children)if(c.name==name)return c; return {}; }
void intTag(Tag& t,int32_t v){t.type=3;t.scalar.clear();put32(t.scalar,(uint32_t)v);} void longTag(Tag&t,int64_t v){t.type=4;t.scalar.clear();put64(t.scalar,(uint64_t)v);}
void byteTag(Tag& t,uint8_t v){t.type=1;t.scalar.assign(1,v);}
void shortTag(Tag& t,int16_t v){t.type=2;t.scalar.clear();put16(t.scalar,(uint16_t)v);}
void floatTag(Tag& t,float v){
 t.type=5;
 t.scalar.resize(4);
 std::uint32_t bits=0;
 std::memcpy(&bits,&v,sizeof(bits));
 put32(t.scalar,bits);
}
int32_t intValue(const Tag& t, int32_t fallback = 0) {
 if (t.type != 3 || t.scalar.size() < 4) return fallback;
 return static_cast<int32_t>(u32(t.scalar.data()));
}
int64_t longValue(const Tag& t, int64_t fallback = 0) {
 if (t.type != 4 || t.scalar.size() < 8) return fallback;
 uint64_t value = 0;
 for (int i = 0; i < 8; ++i) value |= uint64_t(t.scalar[i]) << (i * 8);
 return static_cast<int64_t>(value);
}
double doubleValue(const Tag& t, double fallback = 0.0) {
 if (t.type != 6 || t.scalar.size() < 8) return fallback;
 uint64_t value = 0;
 for (int i = 0; i < 8; ++i) value |= uint64_t(t.scalar[i]) << (i * 8);
 double result = 0.0;
 std::memcpy(&result,&value,sizeof(result));
 return result;
}

fs::path resolveWorldFolder(const fs::path& selected) {
  if (fs::is_regular_file(selected / "level.dat"))
    return selected;
  std::vector<fs::path> candidates;
  if (fs::is_directory(selected)) {
    for (const auto& entry : fs::directory_iterator(selected)) {
      if (!entry.is_directory()) continue;
      const fs::path candidate = entry.path();
      if (fs::is_regular_file(candidate / "level.dat") &&
          (fs::is_regular_file(candidate / "uwp_storage.version") ||
           fs::is_regular_file(candidate / "chunks.dat")))
        candidates.push_back(candidate);
    }
  }
  if (candidates.size() == 1)
    return candidates.front();
  if (!fs::exists(selected / "level.dat"))
    throw std::runtime_error("level.dat was not found in the selected folder (select the actual world folder)");
  return selected;
}
Tag readLevel(const fs::path& p) {
  if (!fs::is_regular_file(p))
    throw std::runtime_error("level.dat was not found in the selected world folder");
  std::ifstream f(p, std::ios::binary);
  if (!f.is_open()) throw std::runtime_error("level.dat could not be opened");
  uint32_t ver=0,size=0;
  f.read((char*)&ver,4); f.read((char*)&size,4);
  need(f.good() && size > 0, "level.dat header is truncated");
  need(ver==3, "source is not a UWP v3 level.dat");
  Bytes b(size); f.read((char*)b.data(),size);
  need((size_t)f.gcount()==size, "level.dat payload is truncated");
  Reader r{b}; Tag root=readNamed(r); need(root.type==10,"invalid source level NBT"); return root;
}
void writeLevel(const fs::path& p,const Tag& src,const std::wstring& display) { Tag out;out.type=10; out.name="";
 auto copy = [&](const char* n){Tag x=child(src,n);if(x.type)out.children.push_back(std::move(x));};
 copy("RandomSeed");copy("GameType");copy("SpawnX");copy("SpawnY");copy("SpawnZ");copy("Time");copy("LastPlayed");copy("Platform");
 Tag dimension;dimension.name="Dimension";intTag(dimension,0);out.children.push_back(dimension);
 Tag generator;generator.name="Generator";intTag(generator,1);out.children.push_back(generator);
 Tag storage;storage.name="StorageVersion";intTag(storage,4);out.children.push_back(storage);
 Tag name;name.name="LevelName";name.type=8;std::string s(display.begin(),display.end());name.scalar.assign(s.begin(),s.end());out.children.push_back(name);
 Bytes nbt=writeRoot(out);std::ofstream f(p,std::ios::binary);uint32_t v=4,z=(uint32_t)nbt.size();f.write((char*)&v,4);f.write((char*)&z,4);f.write((char*)nbt.data(),nbt.size()); }

Tag namedString(const char* name, const std::string& value) {
 Tag t; t.name=name; t.type=8; t.scalar.assign(value.begin(),value.end()); return t;
}
Tag namedInt(const char* name, int32_t value) {
 Tag t; t.name=name; intTag(t,value); return t;
}
Tag namedLong(const char* name, int64_t value) {
 Tag t; t.name=name; longTag(t,value); return t;
}
Tag namedByte(const char* name, uint8_t value) {
 Tag t; t.name=name; byteTag(t,value); return t;
}
Tag namedFloat(const char* name, float value) {
 Tag t; t.name=name; floatTag(t,value); return t;
}

// Declared here because the modern 1.17 record helpers appear before the
// shared LevelDB batch utility's implementation below.
void batchPut(leveldb::WriteBatch& batch,const std::string& k,const Bytes& v,
              size_t& pendingBytes);

constexpr uint32_t Bedrock17BlockStateVersion = 0x000110D2u;
constexpr int32_t Bedrock17NetworkVersion = 471;

// The 1.17.41 reference save stores the flat preset as a JSON string in an
// NBT string tag. Keep the newline: it is present in the game-written value
// and makes byte-for-byte inspection of generated saves less surprising.
const char* EmptyFlatWorldLayers17() {
 return "{\"biome_id\":1,\"block_layers\":[],\"encoding_version\":5,\"structure_options\":null}\n";
}

Bytes make17SkyBiomeData() {
 // BiomeData is a world-level NBT override table.  Keeping biome 12 means
 // the converted Sky chunks remain cold (so existing snow and ice are not
 // turned into a Plains climate), while a zero snowAccumulation value stops
 // the Bedrock snow-accumulation pass from adding new layers there.
 Tag root; root.type=10; root.name="";
 Tag list; list.name="list"; list.type=9; list.listType=10;
 Tag biome; biome.type=10; biome.name="";
 biome.children.push_back(namedByte("id",12));
 biome.children.push_back(namedFloat("snowAccumulation",0.0f));
 list.children.push_back(std::move(biome));
 root.children.push_back(std::move(list));
 return writeRoot(root);
}

void writeLevel17(const fs::path& p, const Tag& src, const std::wstring& display,
                  bool enableWeatherCycle) {
 Tag out; out.type=10; out.name="";
 const int64_t seed = longValue(child(src,"RandomSeed"));
 const int32_t gameType = intValue(child(src,"GameType"),1);
 const int32_t spawnX = intValue(child(src,"SpawnX"),0);
 const int32_t spawnY = intValue(child(src,"SpawnY"),64);
 const int32_t spawnZ = intValue(child(src,"SpawnZ"),0);
 const int64_t time = longValue(child(src,"Time"));
 const int64_t lastPlayed = longValue(child(src,"LastPlayed"));
 const int32_t platform = intValue(child(src,"Platform"),2);
 const Tag sourceSpawnMobs = child(src,"spawnMobs");
 const uint8_t spawnMobs = sourceSpawnMobs.type == 1 && !sourceSpawnMobs.scalar.empty() ?
     sourceSpawnMobs.scalar[0] : 1;
 const std::string name(display.begin(),display.end());

 // These are the fields needed to identify a Bedrock 1.17.41 flat world.
 // Additional player/entity fields are deliberately handled by later steps.
 out.children.push_back(namedString("BiomeOverride", ""));
 out.children.push_back(namedInt("Difficulty", 2));
 out.children.push_back(namedString("FlatWorldLayers", EmptyFlatWorldLayers17()));
 out.children.push_back(namedInt("ForceGameType", gameType));
 out.children.push_back(namedInt("GameType", gameType));
 out.children.push_back(namedInt("Generator", 2));
 out.children.push_back(namedString("InventoryVersion", "1.17.41"));
 out.children.push_back(namedByte("LANBroadcast", 1));
 out.children.push_back(namedByte("LANBroadcastIntent", 1));
 out.children.push_back(namedLong("LastPlayed", lastPlayed));
 out.children.push_back(namedString("LevelName", name));
 out.children.push_back(namedInt("MultiplayerGame", 0));
 out.children.push_back(namedInt("MultiplayerGameIntent", 0));
 out.children.push_back(namedInt("NetworkVersion", Bedrock17NetworkVersion));
 out.children.push_back(namedInt("Platform", platform));
 out.children.push_back(namedLong("RandomSeed", seed));
 out.children.push_back(namedInt("SpawnX", spawnX));
 out.children.push_back(namedInt("SpawnY", spawnY));
 out.children.push_back(namedInt("SpawnZ", spawnZ));
 out.children.push_back(namedByte("SpawnMobs", spawnMobs));
 out.children.push_back(namedInt("StorageVersion", 8));
 out.children.push_back(namedLong("Time", time));
 out.children.push_back(namedInt("WorldVersion", 1));

 if(enableWeatherCycle) {
  // Bedrock stores this gamerule as a root-level byte tag, not inside a
  // GameRules compound. Snow layers are now materialized in the converted
  // chunks, so Sky can retain the normal weather cycle.
  out.children.push_back(namedByte("doweathercycle",1));
 }

 Bytes nbt=writeRoot(out);
 std::ofstream f(p,std::ios::binary);
 need(f.is_open(),"1.17 level.dat could not be opened");
 uint32_t version=8, size=static_cast<uint32_t>(nbt.size());
 f.write((char*)&version,4); f.write((char*)&size,4);
 f.write((char*)nbt.data(),static_cast<std::streamsize>(nbt.size()));
 need(f.good(),"1.17 level.dat could not be written");
}

std::string key17(int x, int z, uint8_t kind, int subchunk = -1) {
 std::string k(8,'\0');
 std::memcpy(k.data(),&x,4);
 std::memcpy(k.data()+4,&z,4);
 k.push_back(static_cast<char>(kind));
 if (subchunk >= 0) k.push_back(static_cast<char>(subchunk));
 return k;
}

Tag make17AirPaletteEntry() {
 Tag root; root.type=10; root.name="";
 root.children.push_back(namedString("name", "minecraft:air"));
 Tag states; states.type=10; states.name="states";
 root.children.push_back(std::move(states));
 root.children.push_back(namedInt("version", static_cast<int32_t>(Bedrock17BlockStateVersion)));
 return root;
}

struct ModernBlockState17 {
 std::string name;
 std::vector<Tag> states;
 // The source id/data pair is deliberately part of this key.  It prevents
 // two old block variants from being merged before their state mapping has
 // been reviewed, which is safer for the first conversion pass.
 std::string key;
};

void add17State(ModernBlockState17& block, Tag state) {
 block.states.push_back(std::move(state));
}

const char* woodType17(uint8_t data) {
 switch (data & 3) {
 case 1: return "spruce";
 case 2: return "birch";
 case 3: return "jungle";
 default: return "oak";
 }
}

const char* stoneType17(uint8_t data) {
 switch (data & 7) {
 case 1: return "granite";
 case 2: return "polished_granite";
 case 3: return "diorite";
 case 4: return "polished_diorite";
 case 5: return "andesite";
 case 6: return "polished_andesite";
 default: return "stone";
 }
}

const char* dirtType17(uint8_t data) {
 switch (data & 3) {
 case 1: return "coarse";
 case 2: return "podzol";
 default: return "normal";
 }
}

const char* sandType17(uint8_t data) {
 return (data & 1) == 0 ? "normal" : "red";
}

const char* sandstoneType17(uint8_t data) {
 switch (data & 3) {
 case 1: return "chiseled";
 case 2: return "smooth";
 default: return "default";
 }
}

const char* stoneBrickType17(uint8_t data) {
 switch (data & 3) {
 case 1: return "mossy";
 case 2: return "cracked";
 case 3: return "chiseled";
 default: return "default";
 }
}

const char* woolColor17(uint8_t data) {
 static const char* const colors[] = {
  "white", "orange", "magenta", "light_blue", "yellow", "lime",
  "pink", "gray", "silver", "cyan", "purple", "blue", "brown",
  "green", "red", "black"
 };
 return colors[data & 15];
}

ModernBlockState17 make17LegacyBlock(uint8_t id, uint8_t data, size_t* unknownBlocks) {
 ModernBlockState17 block;
 block.key=std::to_string(id)+":"+std::to_string(data);
 switch (id) {
 case 0: block.name="minecraft:air"; break;
 case 1: block.name="minecraft:stone"; add17State(block,namedString("stone_type",stoneType17(data))); break;
 case 2: block.name="minecraft:grass"; break;
 case 3: block.name="minecraft:dirt"; add17State(block,namedString("dirt_type",dirtType17(data))); break;
 case 4: block.name="minecraft:cobblestone"; break;
 case 5: block.name="minecraft:planks"; add17State(block,namedString("wood_type",woodType17(data))); break;
 case 6: block.name="minecraft:sapling"; add17State(block,namedString("sapling_type",woodType17(data))); break;
 case 7: block.name="minecraft:bedrock"; break;
 case 8: case 9: block.name="minecraft:water"; add17State(block,namedInt("liquid_depth",data & 15)); break;
 case 10: case 11: block.name="minecraft:lava"; add17State(block,namedInt("liquid_depth",data & 15)); break;
 case 12: block.name="minecraft:sand"; add17State(block,namedString("sand_type",sandType17(data))); break;
 case 13: block.name="minecraft:gravel"; break;
 case 14: block.name="minecraft:gold_ore"; break;
 case 15: block.name="minecraft:iron_ore"; break;
 case 16: block.name="minecraft:coal_ore"; break;
 case 17: block.name="minecraft:log"; add17State(block,namedString("old_log_type",woodType17(data))); break;
 case 18: block.name="minecraft:leaves"; add17State(block,namedString("old_leaf_type",woodType17(data))); break;
 case 20: block.name="minecraft:glass"; break;
 case 21: block.name="minecraft:lapis_ore"; break;
 case 22: block.name="minecraft:lapis_block"; break;
 case 24: block.name="minecraft:sandstone"; add17State(block,namedString("sandstone_type",sandstoneType17(data))); break;
 case 26: block.name="minecraft:bed"; add17State(block,namedInt("direction",data & 3)); add17State(block,namedByte("head_piece_bit",(data & 8) ? 1 : 0)); add17State(block,namedByte("occupied_bit",0)); break;
 case 30: block.name="minecraft:web"; break;
 case 31: block.name="minecraft:tallgrass"; add17State(block,namedString("tall_grass_type",(data & 1) ? "fern" : "default")); break;
 case 35: block.name="minecraft:wool"; add17State(block,namedString("color",woolColor17(data))); break;
 case 37: block.name="minecraft:yellow_flower"; break;
 case 38: block.name="minecraft:red_flower"; add17State(block,namedString("flower_type","poppy")); break;
 case 39: block.name="minecraft:brown_mushroom"; break;
 case 40: block.name="minecraft:red_mushroom"; break;
 case 41: block.name="minecraft:gold_block"; break;
 case 42: block.name="minecraft:iron_block"; break;
 case 43: block.name="minecraft:double_stone_slab"; add17State(block,namedString("stone_slab_type","smooth_stone")); break;
 case 44: block.name="minecraft:stone_slab"; add17State(block,namedString("stone_slab_type","smooth_stone")); break;
 case 45: block.name="minecraft:brick_block"; break;
 case 46: block.name="minecraft:tnt"; break;
 case 47: block.name="minecraft:bookshelf"; break;
 case 48: block.name="minecraft:mossy_cobblestone"; break;
 case 49: block.name="minecraft:obsidian"; break;
 case 50: block.name="minecraft:torch"; add17State(block,namedString("torch_facing_direction","top")); break;
 case 51: block.name="minecraft:fire"; add17State(block,namedInt("age",data & 15)); break;
 case 53: block.name="minecraft:oak_stairs"; add17State(block,namedInt("weirdo_direction",data & 3)); add17State(block,namedByte("upside_down_bit",(data & 4) ? 1 : 0)); break;
 case 54: block.name="minecraft:chest"; add17State(block,namedInt("facing_direction",2)); break;
 case 56: block.name="minecraft:diamond_ore"; break;
 case 57: block.name="minecraft:diamond_block"; break;
 case 58: block.name="minecraft:crafting_table"; break;
 case 59: block.name="minecraft:wheat"; add17State(block,namedInt("growth",data & 7)); break;
 case 60: block.name="minecraft:farmland"; add17State(block,namedInt("moisturized_amount",data & 7)); break;
 case 61: block.name="minecraft:furnace"; add17State(block,namedInt("facing_direction",2)); break;
 case 62: block.name="minecraft:lit_furnace"; add17State(block,namedInt("facing_direction",2)); break;
 case 63: block.name="minecraft:standing_sign"; add17State(block,namedInt("ground_sign_direction",data & 15)); break;
 case 64: block.name="minecraft:wooden_door"; add17State(block,namedInt("direction",data & 3)); add17State(block,namedByte("open_bit",0)); add17State(block,namedByte("upper_block_bit",(data & 8) ? 1 : 0)); break;
 case 65: block.name="minecraft:ladder"; add17State(block,namedInt("facing_direction",data & 7)); break;
 case 67: block.name="minecraft:stone_stairs"; add17State(block,namedInt("weirdo_direction",data & 3)); add17State(block,namedByte("upside_down_bit",(data & 4) ? 1 : 0)); break;
 case 68: block.name="minecraft:wall_sign"; add17State(block,namedInt("facing_direction",data & 7)); break;
 case 71: block.name="minecraft:iron_door"; add17State(block,namedInt("direction",data & 3)); add17State(block,namedByte("open_bit",0)); add17State(block,namedByte("upper_block_bit",(data & 8) ? 1 : 0)); break;
 case 73: block.name="minecraft:redstone_ore"; break;
 case 74: block.name="minecraft:lit_redstone_ore"; break;
 case 78: block.name="minecraft:snow_layer"; add17State(block,namedInt("height",data & 7)); break;
 case 79: block.name="minecraft:ice"; break;
 case 80: block.name="minecraft:snow"; break;
 case 81: block.name="minecraft:cactus"; add17State(block,namedInt("age",data & 15)); break;
 case 82: block.name="minecraft:clay"; break;
 case 83: block.name="minecraft:reeds"; break;
 case 85: block.name="minecraft:fence"; break;
 case 87: block.name="minecraft:netherrack"; break;
 case 89: block.name="minecraft:glowstone"; break;
 case 95: block.name="minecraft:invisibleBedrock"; break;
 case 96: block.name="minecraft:trapdoor"; add17State(block,namedInt("direction",data & 3)); add17State(block,namedByte("open_bit",(data & 4) ? 1 : 0)); add17State(block,namedByte("upside_down_bit",(data & 8) ? 1 : 0)); break;
 case 98: block.name="minecraft:stonebrick"; add17State(block,namedString("stone_brick_type",stoneBrickType17(data))); break;
 case 102: block.name="minecraft:iron_bars"; break;
 case 103: block.name="minecraft:melon_block"; break;
 case 105: block.name="minecraft:melon_stem"; add17State(block,namedInt("growth",data & 7)); break;
 case 107: block.name="minecraft:fence_gate"; add17State(block,namedInt("direction",data & 3)); add17State(block,namedByte("open_bit",(data & 4) ? 1 : 0)); break;
 case 108: block.name="minecraft:brick_stairs"; add17State(block,namedInt("weirdo_direction",data & 3)); add17State(block,namedByte("upside_down_bit",(data & 4) ? 1 : 0)); break;
 case 109: block.name="minecraft:stone_brick_stairs"; add17State(block,namedInt("weirdo_direction",data & 3)); add17State(block,namedByte("upside_down_bit",(data & 4) ? 1 : 0)); break;
 case 112: block.name="minecraft:nether_brick"; break;
 case 114: block.name="minecraft:nether_brick_stairs"; add17State(block,namedInt("weirdo_direction",data & 3)); add17State(block,namedByte("upside_down_bit",(data & 4) ? 1 : 0)); break;
 case 128: block.name="minecraft:sandstone_stairs"; add17State(block,namedInt("weirdo_direction",data & 3)); add17State(block,namedByte("upside_down_bit",(data & 4) ? 1 : 0)); break;
 case 155: block.name="minecraft:quartz_block"; break;
 case 156: block.name="minecraft:quartz_stairs"; add17State(block,namedInt("weirdo_direction",data & 3)); add17State(block,namedByte("upside_down_bit",(data & 4) ? 1 : 0)); break;
 case 245: block.name="minecraft:stonecutter_block"; break;
 case 246: block.name="minecraft:glowingobsidian"; break;
 case 247: block.name="minecraft:netherreactor"; break;
 case 248: case 249: block.name="minecraft:info_update"; break;
 default:
  block.name="minecraft:stone";
  add17State(block,namedString("stone_type","stone"));
  if (unknownBlocks != nullptr) ++*unknownBlocks;
  break;
 }
 return block;
}

Tag make17BlockPaletteEntry(const ModernBlockState17& block) {
 Tag root; root.type=10; root.name="";
 root.children.push_back(namedString("name",block.name));
 Tag states; states.type=10; states.name="states";
 states.children=block.states;
 root.children.push_back(std::move(states));
 root.children.push_back(namedInt("version",static_cast<int32_t>(Bedrock17BlockStateVersion)));
 return root;
}

int bitsFor17Palette(size_t paletteSize) {
 static const int supported[] = {1,2,3,4,5,6,8,16};
 for (const int bits : supported)
  if (paletteSize <= (size_t(1) << bits)) return bits;
 throw std::runtime_error("1.17 subchunk palette has too many block states");
}

Bytes pack17Indices(const std::vector<uint32_t>& indices, int bits) {
 const int perWord=32/bits;
 const uint32_t mask=bits==32 ? 0xffffffffU : ((uint32_t(1) << bits)-1U);
 const size_t wordCount=(indices.size()+static_cast<size_t>(perWord)-1)/static_cast<size_t>(perWord);
 Bytes out;out.reserve(wordCount*4);
 for (size_t word=0;word<wordCount;++word) {
  uint32_t value=0;
  for (int slot=0;slot<perWord;++slot) {
   const size_t index=word*static_cast<size_t>(perWord)+static_cast<size_t>(slot);
   if(index>=indices.size())break;
   value|=(indices[index]&mask) << (slot*bits);
  }
  put32(out,value);
 }
 return out;
}

Bytes make17Subchunk(const Bytes& c, int subchunkY, size_t* unknownBlocks) {
 need(c.size()>=ChunkBytes,"short UWP chunk");
 std::vector<ModernBlockState17> palette;
 std::map<std::string,uint32_t> paletteIndex;
 std::vector<uint32_t> indices;
 indices.reserve(4096);
 for (int x=0;x<16;++x) for (int z=0;z<16;++z) for (int localY=0;localY<16;++localY) {
  const int y=subchunkY*16+localY;
  const size_t pos=static_cast<size_t>(x<<11 | z<<7 | y);
  const uint8_t id=c[pos];
  const size_t dataPos=BlockBytes+(pos>>1);
  const uint8_t data=(pos&1)==0 ? (c[dataPos]&0x0f) : ((c[dataPos]>>4)&0x0f);
  ModernBlockState17 block=make17LegacyBlock(id,data,unknownBlocks);
  auto found=paletteIndex.find(block.key);
  if(found==paletteIndex.end()) {
   const uint32_t newIndex=static_cast<uint32_t>(palette.size());
   paletteIndex.emplace(block.key,newIndex);
   palette.push_back(std::move(block));
   indices.push_back(newIndex);
  } else {
   indices.push_back(found->second);
  }
 }
 const int bits=bitsFor17Palette(palette.size());
 Bytes out{9,1,static_cast<uint8_t>(static_cast<int8_t>(subchunkY)),static_cast<uint8_t>(bits<<1)};
 Bytes packed=pack17Indices(indices,bits);out.insert(out.end(),packed.begin(),packed.end());
 put32(out,static_cast<uint32_t>(palette.size()));
 for(const ModernBlockState17& block:palette) {
  Bytes entry=writeRoot(make17BlockPaletteEntry(block));
  out.insert(out.end(),entry.begin(),entry.end());
 }
 return out;
}

Bytes make17Data2D(const Bytes& c, int chunkX, int chunkZ, const converter::legacy::BiomeSource& biomes) {
 need(c.size()>=ChunkBytes,"short UWP chunk");
 Bytes out;out.reserve(512+256);
 for (int z=0;z<16;++z) for (int x=0;x<16;++x) {
  int height=0;
  const size_t column=static_cast<size_t>(x<<11 | z<<7);
  for(int y=127;y>=0;--y) {
   if(c[column+static_cast<size_t>(y)]!=0) {height=y+1;break;}
  }
  put16(out,static_cast<uint16_t>(height));
 }
 for (int z=0;z<16;++z) for (int x=0;x<16;++x)
  out.push_back(biomes.targetBiomeId(chunkX*16+x,chunkZ*16+z));
 return out;
}

void write17Chunk(leveldb::WriteBatch& batch, int x, int z, const Bytes& c,
                  const converter::legacy::BiomeSource& biomes,
                  size_t& pendingBytes, size_t* unknownBlocks) {
 for(int subchunk=0;subchunk<8;++subchunk) {
  Bytes value=make17Subchunk(c,subchunk,unknownBlocks);
  batchPut(batch,key17(x,z,0x2f,subchunk),value,pendingBytes);
 }
 Bytes version{40};
 batchPut(batch,key17(x,z,0x2c),version,pendingBytes);
 Bytes data2d=make17Data2D(c,x,z,biomes);
 batchPut(batch,key17(x,z,0x2d),data2d,pendingBytes);
 Bytes conversion{2,0,0,0};
 batchPut(batch,key17(x,z,0x36),conversion,pendingBytes);
}

Bytes make17VoidSubchunk() {
 // SubChunkPrefix v9: one storage, palette mode 0, one-bit indices. 4096
 // one-bit air indices occupy 512 bytes. The reference 1.17.41 save uses a
 // four-byte little-endian palette count followed by unnamed NBT palette
 // entries.
 Bytes out{9,1,0,2};
 out.insert(out.end(),512,0);
 put32(out,1);
 Bytes palette=writeRoot(make17AirPaletteEntry());
 out.insert(out.end(),palette.begin(),palette.end());
 return out;
}

void write17VoidChunk(leveldb::WriteBatch& batch, int x, int z, size_t& pendingBytes) {
 Bytes subchunk=make17VoidSubchunk();
 batchPut(batch,key17(x,z,0x2f,0),subchunk,pendingBytes);

 // The 1.17 chunk version record identifies the modern subchunk path.
 Bytes version{40};
 batchPut(batch,key17(x,z,0x2c),version,pendingBytes);

 // Data2D stores 256 little-endian height values followed by 256 biome IDs.
 // A void anchor has height zero and plains as a harmless default biome.
 Bytes data2d(512,0);
 data2d.insert(data2d.end(),256,1);
 batchPut(batch,key17(x,z,0x2d),data2d,pendingBytes);

 // Match the conversion-data marker observed in the reference 1.17.41 save.
 Bytes conversion{2,0,0,0};
 batchPut(batch,key17(x,z,0x36),conversion,pendingBytes);
}

Bytes decode(const uint8_t* p,size_t n) { if(n>=8 && !std::memcmp(p,"MCZ1",4)){uint32_t raw=u32(p+4);Bytes out(raw);uLongf z=raw;need(uncompress(out.data(),&z,p+8,(uLong)(n-8))==Z_OK && z==raw,"zlib chunk decode failed");return out;}return Bytes(p,p+n); }
std::optional<Bytes> regionChunk(const fs::path& file,int lx,int lz) { std::ifstream f(file,std::ios::binary);if(!f)return{};std::array<uint8_t,Header> h{};f.read((char*)h.data(),h.size());uint32_t off=u32(h.data()+4*(lx+lz*32));if(!off)return{};auto sector=off>>8,count=off&255;need(count>0,"bad region offset");f.seekg(std::streamoff(sector)*Sector);uint32_t n=0;f.read((char*)&n,4);need(n>=4 && n<=size_t(count)*Sector,"bad region record");Bytes stored(n-4);f.read((char*)stored.data(),stored.size());return decode(stored.data(),stored.size()); }
struct ChunkRef { fs::path file; int x = 0; int z = 0; };
std::vector<ChunkRef> listRegionChunks(const std::vector<fs::path>& regionFiles) {
 std::vector<ChunkRef> result;
 for (const fs::path& file : regionFiles) {
  const std::string name = file.filename().string();
  int rx = 0, rz = 0;
  if (std::sscanf(name.c_str(),"chunks.r.%d.%d.dat",&rx,&rz)!=2) continue;
  std::ifstream f(file,std::ios::binary);
  if (!f) continue;
  std::array<uint8_t,Header> header{};
  f.read((char*)header.data(),header.size());
  if (static_cast<size_t>(f.gcount()) != header.size()) continue;
  for (int lz=0;lz<32;++lz) for (int lx=0;lx<32;++lx) {
   if (u32(header.data()+4*(lx+lz*32))==0) continue;
   result.push_back(ChunkRef{file,rx*32+lx,rz*32+lz});
  }
 }
 return result;
}
std::vector<ChunkRef> listFiniteChunks(const fs::path& file) {
 std::vector<ChunkRef> result;
 std::ifstream f(file,std::ios::binary);
 if(!f)return result;
 std::array<uint8_t,Header> header{};
 f.read((char*)header.data(),header.size());
 if(static_cast<size_t>(f.gcount())!=header.size())return result;
 // Old/finite worlds occupy the 16x16 chunk area at the beginning of the
 // single chunks.dat region.  Do not interpret unused header slots as world
 // coordinates: those slots are outside the finite 256x256 boundary.
 for(int z=0;z<16;++z) for(int x=0;x<16;++x)
  if(u32(header.data()+4*(x+z*32))!=0)
   result.push_back(ChunkRef{file,x,z});
 return result;
}
using GlobalTileEntities = std::map<std::int64_t,std::vector<Tag>>;
std::int64_t tileEntityChunkKey(int x,int z) {
 return (static_cast<std::int64_t>(static_cast<std::uint32_t>(x))<<32) |
        static_cast<std::uint32_t>(z);
}
bool loadFiniteTileEntities(const fs::path& file,GlobalTileEntities& output) {
 output.clear();
 if(!fs::is_regular_file(file))return true;
 std::ifstream f(file,std::ios::binary);
 if(!f) return false;
 std::array<uint8_t,12> header{};
 f.read((char*)header.data(),header.size());
 if(static_cast<size_t>(f.gcount())!=header.size() ||
    std::memcmp(header.data(),"ENT\0",4)!=0 ||
    u32(header.data()+4)!=1)
  return false;
 const uint32_t size=u32(header.data()+8);
 if(size==0)return true;
 Bytes payload(size);
 f.read((char*)payload.data(),payload.size());
 if(static_cast<size_t>(f.gcount())!=payload.size())return false;
 Reader r{payload};
 Tag root=readNamed(r);
 if(root.type!=10)return false;
 Tag entities=child(root,"TileEntities");
 if(entities.type!=9)return true;
 for(const Tag& entity:entities.children) {
  if(entity.type!=10)continue;
  const Tag xTag=child(entity,"x"),zTag=child(entity,"z");
  if(xTag.type!=3 || zTag.type!=3)continue;
  Tag copy=entity;
  copy.name="";
  output[tileEntityChunkKey(intValue(xTag)/16,intValue(zTag)/16)].push_back(std::move(copy));
 }
 return true;
}
std::string key(int x,int z,uint8_t kind) { std::string k(9,'\0');std::memcpy(k.data(),&x,4);std::memcpy(k.data()+4,&z,4);k[8]=(char)kind;return k; }
void batchPut(leveldb::WriteBatch& batch,const std::string& k,const Bytes& v,size_t& pendingBytes) {
 batch.Put(leveldb::Slice(k),leveldb::Slice((const char*)v.data(),v.size()));
 pendingBytes += k.size() + v.size() + 16;
}
void flushBatch(leveldb::DB* db,leveldb::WriteBatch& batch,size_t& pendingBytes) {
 if (!pendingBytes) return;
 auto s=db->Write(leveldb::WriteOptions(),&batch);
 need(s.ok(),s.ToString().c_str());
 batch.Clear();
 pendingBytes=0;
}
bool blocksLightBlock(uint8_t id) {
 // LevelChunk::recalcHeightmap() tests Tile::lightBlock rather than the
 // block ID itself.  Most ordinary blocks use 255, while these non-solid
 // blocks leave the heightmap at the solid block below them.  Water, ice,
 // leaves and cobwebs have explicit non-zero values in Tile.cpp and are
 // therefore intentionally not listed here.
 switch (id) {
 case 0: case 6: case 20: case 27: case 28: case 31: case 32:
 case 37: case 38: case 39: case 40: case 50: case 51: case 55:
 case 59: case 63: case 64: case 65: case 66: case 68: case 69:
 case 70: case 71: case 72: case 75: case 76: case 77: case 78:
 case 83: case 90: case 92: case 93: case 94: case 96: case 104:
 case 105: case 106: case 111: case 115: case 127: case 131:
 case 132: case 141: case 142: case 143: case 147: case 148:
 case 149: case 150: case 157:
  return false;
 default:
  return true;
 }
}

void appendHeightmap(Bytes& out, const Bytes& c) {
 // UWP block IDs are laid out as blocks[x << 11 | z << 7 | y].  The old
 // LevelChunk implementation stores the first solid Y in each column as a
 // byte, exactly what 0.9's LegacyTerrain expects at offset 81920.
 for (int z = 0; z < 16; ++z) {
  for (int x = 0; x < 16; ++x) {
   int y = 127;
   const size_t column = static_cast<size_t>(x << 11 | z << 7);
   while (y > 0 && !blocksLightBlock(c[column + static_cast<size_t>(y - 1)])) --y;
   out.push_back(static_cast<uint8_t>(y));
  }
 }
}

void setNibble(Bytes& bytes, size_t offset, size_t pos, uint8_t value) {
 size_t slot = offset + (pos >> 1);
 if ((pos & 1) == 0)
  bytes[slot] = uint8_t((bytes[slot] & 0xf0) | (value & 0x0f));
 else
  bytes[slot] = uint8_t((bytes[slot] & 0x0f) | ((value & 0x0f) << 4));
}

struct SeaLevelAdjustment {
 size_t waterRemoved = 0;
 size_t iceRemoved = 0;
 size_t dirtToGrass = 0;
 size_t waterFrozen = 0;
 size_t winterSnowPlaced = 0;
};

SeaLevelAdjustment lowerLegacySeaLevel(Bytes& c, bool freezeWinterWater,
                                       bool addWinterSurfaceSnow) {
 SeaLevelAdjustment result;
 if (c.size() < ChunkBytes) return result;
 auto clearBlock = [&](size_t pos) {
  c[pos] = 0; // air
  setNibble(c, BlockBytes, pos, 0); // block data
  setNibble(c, BlockBytes + NibbleBytes, pos, 15); // exposed sky light
  setNibble(c, BlockBytes + 2 * NibbleBytes, pos, 0); // no block light
 };
 for (int x = 0; x < 16; ++x) {
  for (int z = 0; z < 16; ++z) {
   const size_t column = static_cast<size_t>(x << 11 | z << 7);
   const size_t surface = column + 62;
   const size_t above = column + 63;
   const uint8_t id = c[above];
   // 8 = flowing water, 9 = still water, 79 = ice in the 0.6.1 block table.
   const bool raisedWaterSurface = id == 8 || id == 9 || id == 79;
   if (raisedWaterSurface) {
    // Use the original block above y=62 before clearing it. This avoids
    // turning unrelated dirt into grass when the option is enabled.
    if (c[surface] == 3) {
     c[surface] = 2; // dirt -> grass
     setNibble(c, BlockBytes, surface, 0);
     ++result.dirtToGrass;
    }
    if (id == 8 || id == 9) ++result.waterRemoved;
    else ++result.iceRemoved;
    clearBlock(above);

    // Alpha Winter keeps a snow cap where the old y=63 water/ice surface was
    // removed above solid land. Do this only for the Alpha/Winter conversion
    // path; ordinary Infinite, Sky, and non-Winter Alpha worlds retain the
    // existing clear-water behavior. A liquid at y=62 remains a liquid
    // surface and is intentionally not covered here.
    const uint8_t below = c[surface];
    const bool belowIsWater = below == 8 || below == 9;
    if (addWinterSurfaceSnow && !belowIsWater && blocksLightBlock(below)) {
     c[above] = 78; // one top-snow layer in the LegacyTerrain format
     setNibble(c, BlockBytes, above, 0);
     ++result.winterSnowPlaced;
    }
   }
   // After removing y=63, optionally freeze the y=62 water directly beneath
   // that old water/ice surface in Winter worlds.
   if (freezeWinterWater && raisedWaterSurface &&
       (c[surface] == 8 || c[surface] == 9)) {
    c[surface] = 79; // ice
    setNibble(c, BlockBytes, surface, 0);
    setNibble(c, BlockBytes + NibbleBytes, surface, 15);
    setNibble(c, BlockBytes + 2 * NibbleBytes, surface, 0);
    ++result.waterFrozen;
   }
  }
 }
 return result;
}

size_t addSkyBedrockFloor(Bytes& c) {
 if (c.size() < ChunkBytes) return 0;
 size_t placed = 0;
 for (int x = 0; x < 16; ++x) {
  for (int z = 0; z < 16; ++z) {
   const size_t column = static_cast<size_t>(x << 11 | z << 7);
   for (int y = 0; y <= 4; ++y) {
    const size_t pos = column + static_cast<size_t>(y);
    if (c[pos] != 0) continue; // preserve any existing bottom blocks
    c[pos] = 7; // bedrock
    setNibble(c, BlockBytes, pos, 0);
    setNibble(c, BlockBytes + NibbleBytes, pos, 0);
    setNibble(c, BlockBytes + 2 * NibbleBytes, pos, 0);
    ++placed;
   }
  }
 }
 return placed;
}

size_t addFloatingBedrockFloor(Bytes& c) {
 if (c.size() < ChunkBytes) return 0;
 for (int x = 0; x < 16; ++x) {
  for (int z = 0; z < 16; ++z) {
   const size_t pos = static_cast<size_t>(x << 11 | z << 7);
   c[pos] = 7; // PE 0.9 bedrock
   setNibble(c, BlockBytes, pos, 0);
   setNibble(c, BlockBytes + NibbleBytes, pos, 0);
   setNibble(c, BlockBytes + 2 * NibbleBytes, pos, 0);
  }
 }
 return 16 * 16;
}

bool isSnowyTargetBiome(std::uint8_t id) {
 // 12 is Ice Plains and 30 is Snowy Taiga/Cold Taiga in the target biome
 // table. Both are cold biomes that can receive a persistent snow layer.
 return id == 12 || id == 30;
}

size_t addStableSnowCover(Bytes& c, int chunkX, int chunkZ,
                          const converter::legacy::BiomeSource& biomes) {
 if (c.size() < ChunkBytes) return 0;
 size_t placed = 0;
 for (int x = 0; x < 16; ++x) {
  for (int z = 0; z < 16; ++z) {
   const std::uint8_t biome = biomes.targetBiomeId(
       chunkX * 16 + x, chunkZ * 16 + z);
   if (!isSnowyTargetBiome(biome)) continue;

   const size_t column = static_cast<size_t>(x << 11 | z << 7);
   int top = 127;
   while (top >= 0 && c[column + static_cast<size_t>(top)] == 0) --top;
   if (top < 0 || top >= 127) continue;

   const std::uint8_t support = c[column + static_cast<size_t>(top)];
   // Keep existing snow and full snow blocks. Water, ice and leaves are not
   // valid supports for this stable surface layer in the legacy block table.
   if (support == 8 || support == 9 || support == 10 || support == 11 ||
       support == 18 || support == 78 || support == 79 || support == 80 ||
       !blocksLightBlock(support)) {
    continue;
   }

   const size_t above = column + static_cast<size_t>(top + 1);
   if (c[above] != 0) continue;
   c[above] = 78; // one layer of legacy snow; modern export maps this block.
   setNibble(c, BlockBytes, above, 0);
   setNibble(c, BlockBytes + NibbleBytes, above, 15);
   setNibble(c, BlockBytes + 2 * NibbleBytes, above, 0);
   ++placed;
  }
 }
 return placed;
}

Bytes terrain09(const Bytes& c, int chunkX, int chunkZ, const converter::legacy::BiomeSource& biomes,
               size_t* forcedSnowBiomeColumns = nullptr) {
 need(c.size()>=ChunkBytes,"short UWP chunk");
 // The first 81920 bytes (blocks, data, sky light and block light) are
 // identical between the UWP records and 0.9 LegacyTerrain.  The old code
 // accidentally expanded updateMap here, which shifted every field and made
 // 0.9 regenerate or crash.  Rebuild the actual heightmap and biome colors.
 Bytes o(c.begin(),c.begin()+81920);
 appendHeightmap(o, c);
 for (int z = 0; z < 16; ++z) {
  for (int x = 0; x < 16; ++x) {
    uint8_t id = biomes.targetBiomeId(chunkX * 16 + x, chunkZ * 16 + z);
    const size_t column = static_cast<size_t>(x << 11 | z << 7);
    bool hasSnow = false;
    for (int y = 0; y < 128; ++y) {
     const uint8_t block = c[column + static_cast<size_t>(y)];
     if (block == 78 || block == 80) {
      hasSnow = true;
      break;
     }
    }
    // The legacy generator applied an altitude correction when placing snow,
    // so a warm 2-D biome can still contain old snow. Newer versions validate
    // snow against the biome climate; preserve those columns as Snowy Plains.
    if (!biomes.isFixedPlainsWorld() && hasSnow && id != 12 && id != 30) {
     id = 12;
     if (forcedSnowBiomeColumns != nullptr) ++*forcedSnowBiomeColumns;
    }
   put32be(o, (uint32_t(id) << 24) | (biomes.targetBiomeColor(id) & 0x00ffffffU));
  }
 }
 need(o.size()==83200,"terrain sizing error");return o;
}
size_t saveTileEntities(leveldb::WriteBatch& batch,int x,int z,const Bytes& c,
                         size_t& pendingBytes,
                         const std::vector<Tag>* finiteEntities = nullptr) {
 std::vector<Tag> output;
 std::map<std::tuple<int32_t,int32_t,int32_t>,bool> occupied;

 // A freshly placed empty chest/furnace is deliberately omitted by the
 // 0.6.1 serializer: ChestTileEntity::shouldSave() and
 // FurnaceTileEntity::shouldSave() return false while their inventories are
 // empty. Parse the optional record when present, but do not return early if
 // it is absent; the block scan below reconstructs the missing roots.
 if(c.size()>=ChunkBytes+8){
  const size_t p=ChunkBytes;
  // RakNet::BitStream writes the optional entity header in network order,
  // while the NBT payload that follows is little-endian.
  const uint32_t version=u32be(c.data()+p),n=u32be(c.data()+p+4);
  if(version==2 && p+8+n<=c.size()){
   Bytes nbt(c.begin()+p+8,c.begin()+p+8+n);
   Reader r{nbt};Tag root=readNamed(r);
   Tag tes=child(root,"TileEntities");
   if(root.type==10 && tes.type==9){
    for(const Tag& entity:tes.children){
     if(entity.type!=10)continue;
     Tag copy=entity;copy.name="";
     // 0.9.5 and later normally persist this flag on block entities. It was
     // absent from the 0.6.1 UWP serializer, but adding the canonical value
     // makes the records survive the old-world upgrade path used by 0.14+.
     bool movable=false;
     for(const Tag& childTag:copy.children) if(childTag.name=="isMovable"){movable=true;break;}
     if(!movable){Tag m;m.name="isMovable";byteTag(m,1);copy.children.push_back(std::move(m));}
     // The old ListTag writer emits an empty inventory as TAG_List(TAG_Byte, 0).
     // Newer loaders expect an inventory list whose element type is Compound,
     // even when it has no entries.
     for(Tag& childTag:copy.children)
      if(childTag.name=="Items" && childTag.type==9 && childTag.children.empty()) childTag.listType=10;
     output.push_back(copy);
     const Tag idTag=child(copy,"id");
     const Tag xTag=child(copy,"x"),yTag=child(copy,"y"),zTag=child(copy,"z");
     if(idTag.type==8 && xTag.type==3 && yTag.type==3 && zTag.type==3)
      occupied[std::make_tuple(intValue(xTag),intValue(yTag),intValue(zTag))]=true;
    }
   }
  }
 }

 // Finite StorageVersion 3 worlds keep their TileEntities in one global
 // entities.dat file rather than in the per-chunk UWP suffix.  Move only the
 // entries belonging to this chunk into the 0.9 concatenated 0x31 record.
 if(finiteEntities!=nullptr) {
  for(const Tag& entity:*finiteEntities) {
   if(entity.type!=10)continue;
   Tag copy=entity;
   copy.name="";
   bool movable=false;
   for(const Tag& field:copy.children)
    if(field.name=="isMovable"){movable=true;break;}
   if(!movable){Tag field;field.name="isMovable";byteTag(field,1);copy.children.push_back(std::move(field));}
   for(Tag& field:copy.children)
    if(field.name=="Items" && field.type==9 && field.children.empty())field.listType=10;
   const Tag xTag=child(copy,"x"),yTag=child(copy,"y"),zTag=child(copy,"z");
   if(xTag.type!=3 || yTag.type!=3 || zTag.type!=3)continue;
   const auto pos=std::make_tuple(intValue(xTag),intValue(yTag),intValue(zTag));
   if(occupied.find(pos)!=occupied.end())continue;
   output.push_back(std::move(copy));
   occupied[pos]=true;
  }
 }

 auto stringTag = [](const char* name,const char* value){
  Tag t;t.name=name;t.type=8;
  for(const char* p=value;*p;++p)t.scalar.push_back(static_cast<uint8_t>(*p));
  return t;
 };
 auto makeContainer = [&](const char* id,int32_t wx,int32_t wy,int32_t wz){
  Tag root;root.type=10;root.name="";
  root.children.push_back(stringTag("id",id));
  Tag tx;tx.name="x";intTag(tx,wx);root.children.push_back(std::move(tx));
  Tag ty;ty.name="y";intTag(ty,wy);root.children.push_back(std::move(ty));
  Tag tz;tz.name="z";intTag(tz,wz);root.children.push_back(std::move(tz));
  Tag movable;movable.name="isMovable";byteTag(movable,1);root.children.push_back(std::move(movable));
  Tag items;items.name="Items";items.type=9;items.listType=10;root.children.push_back(std::move(items));
  return root;
 };
 auto makeFurnace = [&](int32_t wx,int32_t wy,int32_t wz){
  Tag root=makeContainer("Furnace",wx,wy,wz);
  Tag burn;burn.name="BurnTime";shortTag(burn,0);root.children.push_back(std::move(burn));
  Tag cook;cook.name="CookTime";shortTag(cook,0);root.children.push_back(std::move(cook));
  return root;
 };

 // Recreate block entities omitted by shouldSave(). IDs are the 0.6.1 values
 // used by Tile::initTiles(): chest=54, furnace=61/62.
 if(c.size()>=ChunkBytes){
  for(int lx=0;lx<16;++lx) for(int lz=0;lz<16;++lz){
   const size_t column=static_cast<size_t>(lx<<11 | lz<<7);
   for(int y=0;y<128;++y){
    const uint8_t block=c[column+static_cast<size_t>(y)];
    if(block!=54 && block!=61 && block!=62)continue;
    const int32_t wx=x*16+lx,wy=y,wz=z*16+lz;
    const auto pos=std::make_tuple(wx,wy,wz);
    if(occupied.find(pos)!=occupied.end())continue;
    output.push_back(block==54?makeContainer("Chest",wx,wy,wz):makeFurnace(wx,wy,wz));
    occupied[pos]=true;
   }
  }
 }

 // 0.9's 0x31 value is a concatenation of unnamed NBT compound roots, one
 // root per block entity. It is not a compound containing a TileEntities list.
 Bytes value;size_t written=0;
 for(const Tag& entity:output){
  Bytes one=writeRoot(entity);value.insert(value.end(),one.begin(),one.end());++written;
 }
 if(!value.empty())batchPut(batch,key(x,z,0x31),value,pendingBytes);
 return written;
}

std::optional<Tag> readChunkEntityRoot(const Bytes& c) {
 if(c.size()<ChunkBytes+8)return{};
 const size_t p=ChunkBytes;
 const uint32_t version=u32be(c.data()+p),n=u32be(c.data()+p+4);
 if(version!=2 || p+8+n>c.size())return{};
 Bytes nbt(c.begin()+p+8,c.begin()+p+8+n);
 Reader r{nbt};
 Tag root=readNamed(r);
 if(root.type!=10)return{};
 return root;
}

size_t saveModernChunkEntities(leveldb::WriteBatch& batch,int x,int z,const Bytes& c,size_t& pendingBytes) {
 std::vector<Tag> blockEntities;
 std::vector<Tag> entities;
 std::map<std::tuple<int32_t,int32_t,int32_t>,bool> occupied;
 const std::optional<Tag> sourceRoot=readChunkEntityRoot(c);
 if(sourceRoot.has_value()) {
  const Tag tileEntities=child(*sourceRoot,"TileEntities");
  if(tileEntities.type==9) {
   for(const Tag& entity:tileEntities.children) {
    if(entity.type!=10)continue;
    Tag copy=entity;copy.name="";
    bool movable=false;
    for(const Tag& field:copy.children) if(field.name=="isMovable"){movable=true;break;}
    if(!movable){Tag field;field.name="isMovable";byteTag(field,1);copy.children.push_back(std::move(field));}
    for(Tag& field:copy.children)
     if(field.name=="Items" && field.type==9 && field.children.empty())field.listType=10;
    const Tag idTag=child(copy,"id");
    const Tag xTag=child(copy,"x"),yTag=child(copy,"y"),zTag=child(copy,"z");
    if(idTag.type==8 && xTag.type==3 && yTag.type==3 && zTag.type==3)
     occupied[std::make_tuple(intValue(xTag),intValue(yTag),intValue(zTag))]=true;
    blockEntities.push_back(std::move(copy));
   }
  }
  const Tag sourceEntities=child(*sourceRoot,"Entities");
  if(sourceEntities.type==9) {
   for(const Tag& entity:sourceEntities.children) {
    if(entity.type!=10)continue;
    Tag copy=entity;copy.name="";
    entities.push_back(std::move(copy));
   }
  }
 }

 // 0.6.1 omits empty container inventories from the serialized suffix.  Keep
 // the same reconstruction used by the PE 0.9 exporter so the 1.17 world
 // does not silently lose chests and furnaces that were placed in old saves.
 if(c.size()>=ChunkBytes) {
  for(int lx=0;lx<16;++lx) for(int lz=0;lz<16;++lz) {
   const size_t column=static_cast<size_t>(lx<<11 | lz<<7);
   for(int y=0;y<128;++y) {
    const uint8_t block=c[column+static_cast<size_t>(y)];
    if(block!=54 && block!=61 && block!=62)continue;
    const int32_t wx=x*16+lx,wy=y,wz=z*16+lz;
    const auto pos=std::make_tuple(wx,wy,wz);
    if(occupied.find(pos)!=occupied.end())continue;
    Tag root;root.type=10;root.name="";
    auto stringTag=[](const char* name,const char* value){
     Tag t;t.name=name;t.type=8;for(const char* p=value;*p;++p)t.scalar.push_back(static_cast<uint8_t>(*p));return t;
    };
    root.children.push_back(stringTag("id",block==54?"Chest":"Furnace"));
    Tag tx;tx.name="x";intTag(tx,wx);root.children.push_back(std::move(tx));
    Tag ty;ty.name="y";intTag(ty,wy);root.children.push_back(std::move(ty));
    Tag tz;tz.name="z";intTag(tz,wz);root.children.push_back(std::move(tz));
    Tag movable;movable.name="isMovable";byteTag(movable,1);root.children.push_back(std::move(movable));
    Tag items;items.name="Items";items.type=9;items.listType=10;root.children.push_back(std::move(items));
    if(block==61 || block==62) {
     Tag burn;burn.name="BurnTime";shortTag(burn,0);root.children.push_back(std::move(burn));
     Tag cook;cook.name="CookTime";shortTag(cook,0);root.children.push_back(std::move(cook));
    }
    blockEntities.push_back(std::move(root));
    occupied[pos]=true;
   }
  }
 }

 Bytes blockValue;size_t written=0;
 for(const Tag& entity:blockEntities) {
  Bytes one=writeRoot(entity);blockValue.insert(blockValue.end(),one.begin(),one.end());++written;
 }
 if(!blockValue.empty())batchPut(batch,key17(x,z,0x31),blockValue,pendingBytes);
 Bytes entityValue;
 for(const Tag& entity:entities) {
  Bytes one=writeRoot(entity);entityValue.insert(entityValue.end(),one.begin(),one.end());
 }
 if(!entityValue.empty())batchPut(batch,key17(x,z,0x32),entityValue,pendingBytes);
 return written+entities.size();
}
}

namespace converter {
bool Convert(const Options& o,const Log& log,std::wstring& error) try {
 fs::path sourceDir=resolveWorldFolder(o.source);
 Tag source=readLevel(sourceDir/"level.dat");
 const int sourceStorageVersion=intValue(child(source,"StorageVersion"));
 const int sourceWorldType=intValue(child(source,"WorldType"));
 const int sourceIndevType=intValue(child(source,"IndevType"));
 // Old and Indev are finite world types. They use the shared chunks.dat
 // storage and do not carry the infinite-world UWP marker file.
 const bool finiteWorld=(sourceWorldType==0 || sourceWorldType==4) &&
  fs::is_regular_file(sourceDir/"chunks.dat");
 if(finiteWorld) {
  if(sourceStorageVersion!=3)
   throw std::runtime_error("finite world conversion requires StorageVersion 3");
 } else if(!fs::exists(sourceDir/"uwp_storage.version")) {
  throw std::runtime_error("selected folder is not a recognized UWP V2 world or StorageVersion 3 finite Old/Indev world");
 }
 if (fs::exists(o.destination)) {
   if (!fs::is_directory(o.destination))
     throw std::runtime_error("destination path is not a folder");
   if (fs::directory_iterator(o.destination) != fs::directory_iterator{})
     throw std::runtime_error("destination folder must be empty");
 } else {
   fs::create_directories(o.destination);
 }
 fs::create_directories(o.destination/"db");
 writeLevel(o.destination/"level.dat",source,o.name.empty()?L"Converted UWP world":o.name);
 leveldb::Options opt;opt.create_if_missing=true;opt.compression=leveldb::kZlibCompression;opt.write_buffer_size=16u*1024u*1024u;opt.max_open_files=256;leveldb::DB* raw=nullptr;auto st=leveldb::DB::Open(opt,(o.destination/"db").string(),&raw);need(st.ok(),st.ToString().c_str());std::unique_ptr<leveldb::DB> db(raw);
 leveldb::WriteBatch batch;size_t pendingBytes=0;
 Tag player=child(source,"Player");if(player.type){player.name="";Bytes value=writeRoot(player);batchPut(batch,"~local_player",value,pendingBytes);}
 const int64_t sourceSeed = longValue(child(source,"RandomSeed"));
 const bool skyWorld = sourceWorldType == 2;
 const bool alphaWorld = sourceWorldType == 3;
 const bool plainsWorld = sourceWorldType == 4 || sourceWorldType == 5;
 const bool winterWorld = alphaWorld && ((static_cast<std::uint64_t>(sourceSeed) & 3ULL) == 0ULL);
 legacy::BiomeSource biomes(sourceSeed,skyWorld,alphaWorld,plainsWorld);
 const bool freezeWinterWater = o.lowerSeaLevel && o.freezeWinterWater && winterWorld;
 const bool skyBedrockFloor = o.skyBedrockFloor && skyWorld;
 const bool floatingIndevWorld = sourceWorldType == 4 && sourceIndevType == 2;
 const bool floatingBedrockFloor = o.floatingBedrockFloor && floatingIndevWorld;

 GlobalTileEntities finiteTileEntities;
 if(finiteWorld && !loadFiniteTileEntities(sourceDir/"entities.dat",finiteTileEntities))
  throw std::runtime_error("finite entities.dat could not be read");

 std::vector<fs::path> regionFiles;
 std::vector<ChunkRef> candidates;
 if(finiteWorld) {
  candidates=listFiniteChunks(sourceDir/"chunks.dat");
 } else {
  for (const auto& entry : fs::directory_iterator(sourceDir)) {
   if (entry.is_regular_file() && entry.path().filename().string().find("chunks.r.") == 0)
    regionFiles.push_back(entry.path());
  }
  std::sort(regionFiles.begin(),regionFiles.end(),[](const fs::path& a,const fs::path& b){return a.filename().wstring()<b.filename().wstring();});
  candidates=listRegionChunks(regionFiles);
 }
 int centerX = intValue(child(source,"SpawnX"));
 int centerZ = intValue(child(source,"SpawnZ"));
 // The player may have travelled far from the original spawn.  Prefer its
 // saved position for the small validation run so 0.9 opens the converted
 // terrain immediately around the player instead of generating fresh chunks.
 Tag sourcePlayer=child(source,"Player");
 Tag position=child(sourcePlayer,"Pos");
 if (position.type==9 && position.children.size()>=3 &&
     position.children[0].type==6 && position.children[2].type==6) {
  centerX=static_cast<int>(std::floor(doubleValue(position.children[0])));
  centerZ=static_cast<int>(std::floor(doubleValue(position.children[2])));
 }
 const int spawnChunkX = centerX >= 0 ? centerX / 16 : (centerX - 15) / 16;
 const int spawnChunkZ = centerZ >= 0 ? centerZ / 16 : (centerZ - 15) / 16;
 std::sort(candidates.begin(),candidates.end(),[&](const ChunkRef& a,const ChunkRef& b){
  const auto distance = [&](const ChunkRef& c) {
   return std::llabs(static_cast<long long>(c.x)-spawnChunkX)+std::llabs(static_cast<long long>(c.z)-spawnChunkZ);
  };
  const auto da=distance(a), db=distance(b);
  return da != db ? da < db : std::pair<int,int>{a.x,a.z} < std::pair<int,int>{b.x,b.z};
 });
 if (o.maxChunks != 0 && candidates.size() > o.maxChunks) candidates.resize(o.maxChunks);
 if (log) {
  std::wstring biomeSourceDescription = finiteWorld ? L"Finite world seed climate" : L"Beta climate noise";
  if (skyWorld) biomeSourceDescription = L"Sky (fixed Ice Plains)";
  else if (alphaWorld) {
   const bool winter = (static_cast<std::uint64_t>(sourceSeed) & 3ULL) == 0ULL;
   biomeSourceDescription = winter ? L"Alpha/Winter (fixed Ice Plains)" : L"Alpha (fixed Plains)";
  }
  else if (plainsWorld) biomeSourceDescription = L"Indev/EarlyInfdev (fixed Plains)";
  log(L"Legacy biome source: "+biomeSourceDescription);
  if (o.maxChunks != 0)
   log(L"Test limit: converting up to "+std::to_wstring(o.maxChunks)+L" existing chunks nearest the saved player/spawn.");
  else
   log(L"Full conversion: all existing chunks will be copied.");
  if(finiteWorld)
   log(sourceWorldType == 4
    ? L"Indev finite StorageVersion 3 source: reading chunks.dat (16x16 chunks) and global entities.dat tile entities."
    : L"Finite StorageVersion 3 source: reading chunks.dat (16x16 chunks) and global entities.dat tile entities.");
  log(L"Selected "+std::to_wstring(candidates.size())+L" existing chunks.");
 }

 size_t converted=0,tileEntities=0,waterRemoved=0,iceRemoved=0,dirtToGrass=0,waterFrozen=0,winterSnowPlaced=0,stableSnowPlaced=0,skyBedrockPlaced=0,floatingBedrockPlaced=0,snowBiomeColumns=0;
 for (const ChunkRef& ref : candidates) {
  const int lx = ((ref.x % 32) + 32) % 32;
  const int lz = ((ref.z % 32) + 32) % 32;
  auto c=regionChunk(ref.file,lx,lz);if(!c)continue;
  if (skyBedrockFloor) skyBedrockPlaced += addSkyBedrockFloor(*c);
  if (floatingBedrockFloor) floatingBedrockPlaced += addFloatingBedrockFloor(*c);
   if (o.lowerSeaLevel) {
    const SeaLevelAdjustment adjustment = lowerLegacySeaLevel(
     *c, freezeWinterWater, o.lowerSeaLevel && winterWorld);
    waterRemoved += adjustment.waterRemoved;
    iceRemoved += adjustment.iceRemoved;
    dirtToGrass += adjustment.dirtToGrass;
    waterFrozen += adjustment.waterFrozen;
    winterSnowPlaced += adjustment.winterSnowPlaced;
   }
  stableSnowPlaced += addStableSnowCover(*c,ref.x,ref.z,biomes);
  // 0.9.5/0.14's last LegacyTerrain version is 2.  Version 1 is the
  // intermediate 0.9.2 value and makes later clients take a different
  // migration branch for old block entities.
  Bytes version{2};batchPut(batch,key(ref.x,ref.z,0x76),version,pendingBytes);
  Bytes terrain=terrain09(*c,ref.x,ref.z,biomes,&snowBiomeColumns);batchPut(batch,key(ref.x,ref.z,0x30),terrain,pendingBytes);
  const std::vector<Tag>* finiteEntities=nullptr;
  if(finiteWorld) {
   const auto it=finiteTileEntities.find(tileEntityChunkKey(ref.x,ref.z));
   if(it!=finiteTileEntities.end())finiteEntities=&it->second;
  }
  tileEntities+=saveTileEntities(batch,ref.x,ref.z,*c,pendingBytes,finiteEntities);++converted;
  if(pendingBytes>=BatchLimit)flushBatch(db.get(),batch,pendingBytes);
 }
 flushBatch(db.get(),batch,pendingBytes);db->CompactRange(nullptr,nullptr);
 std::ofstream(o.destination/"levelname.txt") << std::string(o.name.begin(),o.name.end());
  log(L"Converted "+std::to_wstring(converted)+L" chunks and "+std::to_wstring(tileEntities)+L" block entities using zlib-compressed LegacyTerrain. "+
      std::to_wstring(waterRemoved)+L" water and "+std::to_wstring(iceRemoved)+L" ice blocks at y=63 were cleared; "+
      std::to_wstring(dirtToGrass)+L" dirt blocks at y=62 became grass; "+std::to_wstring(waterFrozen)+
      L" water blocks at y=62 were frozen for Winter; "+std::to_wstring(skyBedrockPlaced)+
      L" Sky air blocks at y=0..4 became bedrock; "+std::to_wstring(floatingBedrockPlaced)+
      L" Indev Floating blocks at y=0 were replaced with bedrock; "+std::to_wstring(snowBiomeColumns)+
      L" snow-bearing columns were mapped to cold biomes; "+std::to_wstring(winterSnowPlaced)+
      L" Winter surface snow layers were placed; "+std::to_wstring(stableSnowPlaced)+
      L" stable snow layers were written for cold-biome columns. Non-player entities were intentionally skipped.");return true;
 } catch(const std::exception& e){error.assign(e.what(),e.what()+std::strlen(e.what()));return false;}

bool Convert17(const Options17& o,const Log& log,std::wstring& error) try {
 fs::path sourceDir=resolveWorldFolder(o.source);
 if(!fs::exists(sourceDir/"uwp_storage.version"))
  throw std::runtime_error("selected folder is not a recognized UWP V2 infinite/Sky/Alpha world");
 if (fs::exists(o.destination)) {
  if (!fs::is_directory(o.destination))
   throw std::runtime_error("destination path is not a folder");
  if (fs::directory_iterator(o.destination) != fs::directory_iterator{})
   throw std::runtime_error("destination folder must be empty");
 } else {
  fs::create_directories(o.destination);
 }
 fs::create_directories(o.destination/"db");

 Tag source=readLevel(sourceDir/"level.dat");
 const int sourceWorldType=intValue(child(source,"WorldType"));
 const bool skyWorld=sourceWorldType==2;
 const bool alphaWorld=sourceWorldType==3;
 const bool plainsWorld=sourceWorldType==4 || sourceWorldType==5;
 const std::wstring display=o.name.empty()?L"Converted Bedrock 1.17.41 void world":o.name;
 writeLevel17(o.destination/"level.dat",source,display,skyWorld);

 leveldb::Options opt;
 opt.create_if_missing=true;
 opt.compression=leveldb::kZlibCompression;
 opt.write_buffer_size=16u*1024u*1024u;
 opt.max_open_files=256;
 leveldb::DB* raw=nullptr;
 auto st=leveldb::DB::Open(opt,(o.destination/"db").string(),&raw);
 need(st.ok(),st.ToString().c_str());
 std::unique_ptr<leveldb::DB> db(raw);

  leveldb::WriteBatch batch;
  size_t pendingBytes=0;
  Tag player=child(source,"Player");
  if(player.type){
   player.name="";
   Bytes value=writeRoot(player);
   batchPut(batch,"~local_player",value,pendingBytes);
  }

  const int64_t sourceSeed=longValue(child(source,"RandomSeed"));
  legacy::BiomeSource biomes(sourceSeed,skyWorld,alphaWorld,plainsWorld);

  if(skyWorld) {
   // Do not remap Sky to Plains: the existing snow/ice blocks must retain a
   // cold biome.  This world-level override only disables future snow-layer
   // accumulation for biome 12 in the converted Sky world.
   batchPut(batch,"BiomeData",make17SkyBiomeData(),pendingBytes);
   if(log)
    log(L"Disabled automatic snow accumulation for Sky biome 12 while preserving its cold climate and existing snow layers.");
  }

  if(o.writeVoidAnchorChunk) {
   const int spawnX=intValue(child(source,"SpawnX"));
   const int spawnZ=intValue(child(source,"SpawnZ"));
   const int chunkX=spawnX>=0 ? spawnX/16 : (spawnX-15)/16;
   const int chunkZ=spawnZ>=0 ? spawnZ/16 : (spawnZ-15)/16;
   write17VoidChunk(batch,chunkX,chunkZ,pendingBytes);
   if(log)
    log(L"Wrote one explicit Bedrock 1.17.41 void anchor chunk at " +
        std::to_wstring(chunkX) + L", " + std::to_wstring(chunkZ) + L".");
  }

  std::vector<fs::path> regionFiles;
  for(const auto& entry:fs::directory_iterator(sourceDir)) {
   if(entry.is_regular_file() && entry.path().filename().string().find("chunks.r.")==0)
    regionFiles.push_back(entry.path());
  }
  std::sort(regionFiles.begin(),regionFiles.end(),[](const fs::path& a,const fs::path& b){
   return a.filename().wstring()<b.filename().wstring();
  });
  std::vector<ChunkRef> candidates=listRegionChunks(regionFiles);
  const int spawnX=intValue(child(source,"SpawnX"));
  const int spawnZ=intValue(child(source,"SpawnZ"));
  if(o.maxChunks!=0) {
   Tag sourcePlayer=child(source,"Player");
   Tag position=child(sourcePlayer,"Pos");
   int centerX=spawnX,centerZ=spawnZ;
   if(position.type==9 && position.children.size()>=3 &&
      position.children[0].type==6 && position.children[2].type==6) {
    centerX=static_cast<int>(std::floor(doubleValue(position.children[0])));
    centerZ=static_cast<int>(std::floor(doubleValue(position.children[2])));
   }
   const int playerChunkX=centerX>=0 ? centerX/16 : (centerX-15)/16;
   const int playerChunkZ=centerZ>=0 ? centerZ/16 : (centerZ-15)/16;
   std::sort(candidates.begin(),candidates.end(),[&](const ChunkRef& a,const ChunkRef& b){
    const auto distance=[&](const ChunkRef& c){
     return std::llabs(static_cast<long long>(c.x)-playerChunkX)+
            std::llabs(static_cast<long long>(c.z)-playerChunkZ);
    };
    const auto da=distance(a),db=distance(b);
    return da!=db ? da<db : std::pair<int,int>{a.x,a.z}<std::pair<int,int>{b.x,b.z};
   });
   if(candidates.size()>o.maxChunks)candidates.resize(o.maxChunks);
  }
  size_t converted=0,blockEntities=0,unknownBlocks=0,stableSnowPlaced=0;
  if(log) {
   if(o.maxChunks!=0)
    log(L"Test limit: converting up to "+std::to_wstring(o.maxChunks)+
        L" existing chunks nearest the saved player/spawn.");
   log(L"Found "+std::to_wstring(candidates.size())+
       L" existing 0.6.1 chunks for modern conversion.");
  }
  for(const ChunkRef& ref:candidates) {
   const int lx=((ref.x%32)+32)%32;
   const int lz=((ref.z%32)+32)%32;
   auto c=regionChunk(ref.file,lx,lz);
   if(!c)continue;
   stableSnowPlaced += addStableSnowCover(*c,ref.x,ref.z,biomes);
   write17Chunk(batch,ref.x,ref.z,*c,biomes,pendingBytes,&unknownBlocks);
   blockEntities+=saveModernChunkEntities(batch,ref.x,ref.z,*c,pendingBytes);
   ++converted;
   if(pendingBytes>=BatchLimit)flushBatch(db.get(),batch,pendingBytes);
  }
 flushBatch(db.get(),batch,pendingBytes);
 db->CompactRange(nullptr,nullptr);

 std::ofstream nameFile(o.destination/"levelname.txt",std::ios::binary);
 need(nameFile.is_open(),"1.17 levelname.txt could not be opened");
 const std::string narrowName(display.begin(),display.end());
 nameFile.write(narrowName.data(),static_cast<std::streamsize>(narrowName.size()));
 need(nameFile.good(),"1.17 levelname.txt could not be written");

 if (log) {
  log(L"Wrote Bedrock 1.17.41 level.dat (StorageVersion 8, Generator 2).");
  log(L"Wrote an empty FlatWorldLayers preset for a pure-void world.");
   log(L"Converted "+std::to_wstring(converted)+L" existing chunks to SubChunkPrefix palettes.");
   log(L"Wrote "+std::to_wstring(stableSnowPlaced)+L" stable snow layers for cold-biome columns.");
   log(L"Copied/reconstructed "+std::to_wstring(blockEntities)+L" block entities and legacy entities.");
   if(unknownBlocks!=0)
    log(L"Mapped "+std::to_wstring(unknownBlocks)+L" unsupported legacy block states to stone.");
 }
 return true;
 } catch(const std::exception& e){error.assign(e.what(),e.what()+std::strlen(e.what()));return false;}
}
