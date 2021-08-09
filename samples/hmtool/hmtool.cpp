#include <SDL.h>
#include <libtcod.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include <libtcod/gui/gui.hpp>

#include "operation.hpp"

TCODHeightMap* hm = NULL;
static TCODHeightMap* hm_old = NULL;
TCODNoise* noise = NULL;
TCODRandom* rnd = NULL;
static TCODRandom* backupRnd = NULL;
static TCODConsole* guicon = NULL;
static bool greyscale = false;
static bool slope = false;
static bool normal = false;
bool isNormalized = true;
static bool oldNormalized = true;
static char msg[512] = "";
float msgDelay = 0.0f;
static constexpr float hillRadius = 0.1f;
static constexpr float hillVariation = 0.5f;
float addFbmDelta = 0.0f;
float scaleFbmDelta = 0.0f;
uint32_t seed = 0xdeadbeef;

static float sandHeight = 0.12f;
static float grassHeight = 0.315f;
static float snowHeight = 0.785f;

static char landMassTxt[128] = "";
static char minZTxt[128] = "";
static char maxZTxt[128] = "";
static char seedTxt[128] = "";
float mapmin = 0.0f, mapmax = 0.0f;
static float oldmapmin = 0.0f, oldmapmax = 0.0f;

// ui
ToolBar* params = NULL;
ToolBar* history = NULL;
ToolBar* colorMapGui = NULL;

static float voronoiCoef[2] = {-1.0f, 1.0f};

static TCODColor mapGradient[256];
static constexpr auto MAX_COLOR_KEY = 10;

// TCOD's land color map
static int keyIndex[MAX_COLOR_KEY] = {
    0,
    (int)(sandHeight * 255),
    (int)(sandHeight * 255) + 4,
    (int)(grassHeight * 255),
    (int)(grassHeight * 255) + 10,
    (int)(snowHeight * 255),
    (int)(snowHeight * 255) + 10,
    255};
static TCODColor keyColor[MAX_COLOR_KEY] = {
    TCODColor(0, 0, 50),       // deep water
    TCODColor(30, 30, 170),    // water-sand transition
    TCODColor(114, 150, 71),   // sand
    TCODColor(80, 120, 10),    // sand-grass transition
    TCODColor(17, 109, 7),     // grass
    TCODColor(120, 220, 120),  // grass-snow transistion
    TCODColor(208, 208, 239),  // snow
    TCODColor(255, 255, 255)};
static Image* keyImages[MAX_COLOR_KEY];

static constexpr int nbColorKeys = 8;

static constexpr auto BLACK = TCODColor{0, 0, 0};
static constexpr auto WHITE = TCODColor{255, 255, 255};
static constexpr auto LIGHT_BLUE = TCODColor{63, 63, 255};

void initColors() { TCODColor::genMap(mapGradient, nbColorKeys, keyColor, keyIndex); }

void render() {
  TCODHeightMap backup(HM_WIDTH, HM_HEIGHT);
  isNormalized = true;
  TCODConsole::root->setDefaultBackground(BLACK);
  TCODConsole::root->setDefaultForeground(WHITE);
  TCODConsole::root->clear();
  backup.copy(hm);
  mapmin = 1E8f;
  mapmax = -1E8f;
  for (int i = 0; i < HM_WIDTH * HM_HEIGHT; i++) {
    if (hm->values[i] < 0.0f || hm->values[i] > 1.0f) {
      isNormalized = false;
    }
    if (hm->values[i] < mapmin) mapmin = hm->values[i];
    if (hm->values[i] > mapmax) mapmax = hm->values[i];
  }
  if (!isNormalized) backup.normalize();
  // render the TCODHeightMap
  for (int x = 0; x < HM_WIDTH; x++) {
    for (int y = 0; y < HM_HEIGHT; y++) {
      float z = backup.getValue(x, y);
      uint8_t val = (uint8_t)(z * 255);
      if (slope) {
        // render the slope map
        z = CLAMP(0.0f, 1.0f, hm->getSlope(x, y) * 10.0f);
        val = (uint8_t)(z * 255);
        TCODColor c(val, val, val);
        TCODConsole::root->setCharBackground(x, y, c);
      } else if (greyscale) {
        // render the greyscale heightmap
        TCODColor c(val, val, val);
        TCODConsole::root->setCharBackground(x, y, c);
      } else if (normal) {
        // render the normal map
        float n[3];
        hm->getNormal((float)x, (float)y, n, mapmin);
        uint8_t r = (uint8_t)((n[0] * 0.5f + 0.5f) * 255);
        uint8_t g = (uint8_t)((n[1] * 0.5f + 0.5f) * 255);
        uint8_t b = (uint8_t)((n[2] * 0.5f + 0.5f) * 255);
        TCODColor c(r, g, b);
        TCODConsole::root->setCharBackground(x, y, c);
      } else {
        // render the colored heightmap
        TCODConsole::root->setCharBackground(x, y, mapGradient[val]);
      }
    }
  }
  snprintf(minZTxt, sizeof(minZTxt), "min z    : %.2f", mapmin);
  snprintf(maxZTxt, sizeof(maxZTxt), "max z    : %.2f", mapmax);
  snprintf(seedTxt, sizeof(seedTxt), "seed     : %X", seed);
  float landProportion = 100.0f - 100.0f * backup.countCells(0.0f, sandHeight) / (hm->w * hm->h);
  snprintf(landMassTxt, sizeof(landMassTxt), "landMass : %d %%", (int)landProportion);
  if (!isNormalized)
    TCODConsole::root->printf(HM_WIDTH / 2, HM_HEIGHT - 1, TCOD_BKGND_NONE, TCOD_CENTER, "the map is not normalized !");
  // message
  msgDelay -= TCODSystem::getLastFrameLength();
  if (msg[0] != 0 && msgDelay > 0.0f) {
    int h = TCODConsole::root->printRectEx(
        HM_WIDTH / 2, HM_HEIGHT / 2 + 1, HM_WIDTH / 2 - 2, 0, TCOD_BKGND_NONE, TCOD_CENTER, "%s", msg);
    TCODConsole::root->setDefaultBackground(LIGHT_BLUE);
    if (h > 0) TCODConsole::root->rect(HM_WIDTH / 4, HM_HEIGHT / 2, HM_WIDTH / 2, h + 2, false, TCOD_BKGND_SET);
    TCODConsole::root->setDefaultBackground(BLACK);
  }
}

void message(float delay, const char* fmt, ...) {
  va_list ap;
  msgDelay = delay;
  va_start(ap, fmt);
  vsprintf(msg, fmt, ap);
  va_end(ap);
}

void backup() {
  // save the heightmap & RNG states
  for (int x = 0; x < HM_WIDTH; x++) {
    for (int y = 0; y < HM_HEIGHT; y++) {
      hm_old->setValue(x, y, hm->getValue(x, y));
    }
  }
  if (backupRnd) delete backupRnd;
  backupRnd = rnd->save();
  oldNormalized = isNormalized;
  oldmapmax = mapmax;
  oldmapmin = mapmin;
}

void restore() {
  // restore the previously saved heightmap & RNG states
  for (int x = 0; x < HM_WIDTH; x++) {
    for (int y = 0; y < HM_HEIGHT; y++) {
      hm->setValue(x, y, hm_old->getValue(x, y));
    }
  }
  rnd->restore(backupRnd);
  isNormalized = oldNormalized;
  mapmax = oldmapmax;
  mapmin = oldmapmin;
}

void save() {
  // TODO
  message(2.0f, "Saved.");
}

void load() {
  // TODO
}

void addHill(int nbHill, float baseRadius, float radiusVar, float height) {
  for (int i = 0; i < nbHill; i++) {
    float hillMinRadius = baseRadius * (1.0f - radiusVar);
    float hillMaxRadius = baseRadius * (1.0f + radiusVar);
    float radius = rnd->getFloat(hillMinRadius, hillMaxRadius);
    float theta = rnd->getFloat(0.0f, 6.283185f);  // between 0 and 2Pi
    float dist = rnd->getFloat(0.0f, (float)MIN(HM_WIDTH, HM_HEIGHT) / 2 - radius);
    int xh = (int)(HM_WIDTH / 2 + cos(theta) * dist);
    int yh = (int)(HM_HEIGHT / 2 + sin(theta) * dist);
    hm->addHill((float)xh, (float)yh, radius, height);
  }
}

void clearCbk(Widget*, void*) {
  hm->clear();
  Operation::clear();
  history->clear();
  params->clear();
  params->setVisible(false);
}

void reseedCbk(Widget*, void*) {
  seed = rnd->getInt(0x7FFFFFFF, 0xFFFFFFFF);
  Operation::reseed();
  message(3.0f, "Switching to seed %X", seed);
}

void cancelCbk(Widget*, void*) { Operation::cancel(); }

// operations buttons callbacks
void normalizeCbk(Widget*, void*) { (new NormalizeOperation(0.0f, 1.0f))->add(); }

void addFbmCbk(Widget*, void*) { (new AddFbmOperation(1.0f, addFbmDelta, 0.0f, 6.0f, 1.0f, 0.5f))->add(); }

void scaleFbmCbk(Widget*, void*) { (new ScaleFbmOperation(1.0f, addFbmDelta, 0.0f, 6.0f, 1.0f, 0.5f))->add(); }

void addHillCbk(Widget*, void*) {
  (new AddHillOperation(25, 10.0f, 0.5f, (mapmax == mapmin ? 0.5f : (mapmax - mapmin) * 0.1f)))->add();
}

void rainErosionCbk(Widget*, void*) { (new RainErosionOperation(1000, 0.05f, 0.05f))->add(); }

void smoothCbk(Widget*, void*) { (new SmoothOperation(mapmin, mapmax, 2))->add(); }

void voronoiCbk(Widget*, void*) { (new VoronoiOperation(100, 2, voronoiCoef))->add(); }

void noiseLerpCbk(Widget*, void*) {
  float v = (mapmax - mapmin) * 0.5f;
  if (v == 0.0f) v = 1.0f;
  (new NoiseLerpOperation(0.0f, 1.0f, addFbmDelta, 0.0f, 6.0f, v, v))->add();
}

void raiseLowerCbk(Widget*, void*) { (new AddLevelOperation(0.0f))->add(); }

// In/Out buttons callbacks

void exportCCbk(Widget*, void*) {
  const char* code = Operation::buildCode(Operation::C);
  FILE* f = fopen("hm.c", "wt");
  fprintf(f, "%s", code);
  fclose(f);
  message(3.0f, "The code has been exported to ./hm.c");
}

void exportPyCbk(Widget*, void*) {
  const char* code = Operation::buildCode(Operation::PY);
  FILE* f = fopen("hm.py", "wt");
  fprintf(f, "%s", code);
  fclose(f);
  message(3.0f, "The code has been exported to ./hm.py");
}

void exportCppCbk(Widget*, void*) {
  const char* code = Operation::buildCode(Operation::CPP);
  FILE* f = fopen("hm.cpp", "wt");
  fprintf(f, "%s", code);
  fclose(f);
  message(3.0f, "The code has been exported to ./hm.cpp");
}

void exportBmpCbk(Widget*, void*) {
  TCODImage img(HM_WIDTH, HM_HEIGHT);
  for (int x = 0; x < HM_WIDTH; x++) {
    for (int y = 0; y < HM_HEIGHT; y++) {
      float z = hm->getValue(x, y);
      uint8_t val = (uint8_t)(z * 255);
      if (slope) {
        // render the slope map
        z = CLAMP(0.0f, 1.0f, hm->getSlope(x, y) * 10.0f);
        val = (uint8_t)(z * 255);
        TCODColor c(val, val, val);
        img.putPixel(x, y, c);
      } else if (greyscale) {
        // render the greyscale heightmap
        TCODColor c(val, val, val);
        img.putPixel(x, y, c);
      } else if (normal) {
        // render the normal map
        float n[3];
        hm->getNormal((float)x, (float)y, n, mapmin);
        uint8_t r = (uint8_t)((n[0] * 0.5f + 0.5f) * 255);
        uint8_t g = (uint8_t)((n[1] * 0.5f + 0.5f) * 255);
        uint8_t b = (uint8_t)((n[2] * 0.5f + 0.5f) * 255);
        TCODColor c(r, g, b);
        img.putPixel(x, y, c);
      } else {
        // render the colored heightmap
        img.putPixel(x, y, mapGradient[val]);
      }
    }
  }
  img.save("hm.bmp");
  message(3.0f, "The bitmap has been exported to ./hm.bmp");
}

// Display buttons callbacks
void colorMapCbk(Widget*, void*) {
  slope = false;
  greyscale = false;
  normal = false;
}

void greyscaleCbk(Widget*, void*) {
  slope = false;
  greyscale = true;
  normal = false;
}

void slopeCbk(Widget*, void*) {
  slope = true;
  greyscale = false;
  normal = false;
}

void normalCbk(Widget*, void*) {
  slope = false;
  greyscale = false;
  normal = true;
}

void changeColorMapIdxCbk(Widget*, float val, void* data) {
  auto i = reinterpret_cast<intptr_t>(data);
  keyIndex[i] = (int)(val);
  if (i == 1) sandHeight = (float)(i) / 255.0f;
  initColors();
}

void changeColorMapRedCbk(Widget*, float val, void* data) {
  auto i = reinterpret_cast<intptr_t>(data);
  keyColor[i].r = static_cast<uint8_t>(val);
  keyImages[i]->setBackgroundColor(keyColor[i]);
  initColors();
}

void changeColorMapGreenCbk(Widget*, float val, void* data) {
  auto i = reinterpret_cast<intptr_t>(data);
  keyColor[i].g = static_cast<uint8_t>(val);
  keyImages[i]->setBackgroundColor(keyColor[i]);
  initColors();
}

void changeColorMapBlueCbk(Widget*, float val, void* data) {
  auto i = reinterpret_cast<intptr_t>(data);
  keyColor[i].b = static_cast<uint8_t>(val);
  keyImages[i]->setBackgroundColor(keyColor[i]);
  initColors();
}

void changeColorMapEndCbk(Widget*, void*) { colorMapGui->setVisible(false); }

void changeColorMapCbk(Widget* w, void*) {
  colorMapGui->move(w->x + w->w + 2, w->y);
  colorMapGui->clear();
  for (intptr_t i = 0; i < nbColorKeys; i++) {
    char tmp[64];
    sprintf(tmp, "Color %d", static_cast<int>(i));
    colorMapGui->addSeparator(tmp);
    HBox* h_box = new HBox(0, 0, 0);
    VBox* v_box = new VBox(0, 0, 0);
    colorMapGui->addWidget(h_box);
    Slider* idxSlider = new Slider(0, 0, 3, 0.0f, 255.0f, "index", "Index of the key in the color map (0-255)");
    idxSlider->setValue((float)keyIndex[i]);
    idxSlider->setFormat("%.0f");
    idxSlider->setCallback(changeColorMapIdxCbk, reinterpret_cast<void*>(i));
    v_box->addWidget(idxSlider);
    keyImages[i] = new Image(0, 0, 0, 2);
    keyImages[i]->setBackgroundColor(keyColor[i]);
    v_box->addWidget(keyImages[i]);
    h_box->addWidget(v_box);

    v_box = new VBox(0, 0, 0);
    h_box->addWidget(v_box);
    Slider* redSlider = new Slider(0, 0, 3, 0.0f, 255.0f, "r", "Red component of the color");
    redSlider->setValue((float)keyColor[i].r);
    redSlider->setFormat("%.0f");
    redSlider->setCallback(changeColorMapRedCbk, reinterpret_cast<void*>(i));
    v_box->addWidget(redSlider);
    Slider* greenSlider = new Slider(0, 0, 3, 0.0f, 255.0f, "g", "Green component of the color");
    greenSlider->setValue((float)keyColor[i].g);
    greenSlider->setFormat("%.0f");
    greenSlider->setCallback(changeColorMapGreenCbk, reinterpret_cast<void*>(i));
    v_box->addWidget(greenSlider);
    Slider* blueSlider = new Slider(0, 0, 3, 0.0f, 255.0f, "b", "Blue component of the color");
    blueSlider->setValue((float)keyColor[i].b);
    blueSlider->setFormat("%.0f");
    blueSlider->setCallback(changeColorMapBlueCbk, reinterpret_cast<void*>(i));
    v_box->addWidget(blueSlider);
  }
  colorMapGui->addWidget(new Button("Ok", NULL, changeColorMapEndCbk, NULL));
  colorMapGui->setVisible(true);
}

// build gui

void addOperationButton(ToolBar* tools, Operation::OpType type, void (*cbk)(Widget* w, void* data)) {
  tools->addWidget(new Button(Operation::names[type], Operation::tips[type], cbk, NULL));
}

void buildGui() {
  // status bar
  new StatusBar(0, 0, HM_WIDTH, 1);

  VBox* vbox = new VBox(0, 2, 1);
  // stats
  ToolBar* stats = new ToolBar(0, 0, 21, "Stats", "Statistics about the current map");
  stats->addWidget(new Label(0, 0, landMassTxt, "Ratio of land surface / total surface"));
  stats->addWidget(new Label(0, 0, minZTxt, "Minimum z value in the map"));
  stats->addWidget(new Label(0, 0, maxZTxt, "Maximum z value in the map"));
  stats->addWidget(new Label(0, 0, seedTxt, "Current random seed used to build the map"));
  vbox->addWidget(stats);

  // tools
  ToolBar* tools = new ToolBar(0, 0, 15, "Tools", "Tools to modify the heightmap");
  tools->addWidget(new Button("cancel", "Delete the selected operation", cancelCbk, NULL));
  tools->addWidget(new Button("clear", "Remove all operations and reset all heightmap values to 0.0", clearCbk, NULL));
  tools->addWidget(new Button("reseed", "Replay all operations with a new random seed", reseedCbk, NULL));

  // operations
  tools->addSeparator("Operations", "Apply a new operation to the map");
  addOperationButton(tools, Operation::NORM, normalizeCbk);
  addOperationButton(tools, Operation::ADD_FBM, addFbmCbk);
  addOperationButton(tools, Operation::SCALE_FBM, scaleFbmCbk);
  addOperationButton(tools, Operation::ADDHILL, addHillCbk);
  addOperationButton(tools, Operation::RAIN, rainErosionCbk);
  addOperationButton(tools, Operation::SMOOTH, smoothCbk);
  addOperationButton(tools, Operation::VORONOI, voronoiCbk);
  addOperationButton(tools, Operation::NOISE_LERP, noiseLerpCbk);
  addOperationButton(tools, Operation::ADDLEVEL, raiseLowerCbk);

  // display
  tools->addSeparator("Display", "Change the type of display");
  RadioButton::setDefaultGroup(1);
  RadioButton* colormap = new RadioButton("colormap", "Enable colormap mode", colorMapCbk, NULL);
  tools->addWidget(colormap);
  colormap->select();
  tools->addWidget(new RadioButton("slope", "Enable slope mode", slopeCbk, NULL));
  tools->addWidget(new RadioButton("greyscale", "Enable greyscale mode", greyscaleCbk, NULL));
  tools->addWidget(new RadioButton("normal", "Enable normal map mode", normalCbk, NULL));
  tools->addWidget(new Button("change colormap", "Modify the colormap used by hmtool", changeColorMapCbk, NULL));

  // change colormap gui
  colorMapGui = new ToolBar(0, 0, "Colormap", "Select the color and position of the keys in the color map");
  colorMapGui->setVisible(false);

  // in/out
  tools->addSeparator("In/Out", "Import/Export stuff");
  tools->addWidget(new Button("export C", "Generate the C code for this heightmap in ./hm.c", exportCCbk, NULL));
  tools->addWidget(
      new Button("export CPP", "Generate the CPP code for this heightmap in ./hm.cpp", exportCppCbk, NULL));
  tools->addWidget(
      new Button("export PY", "Generate the Python code for this heightmap in ./hm.py", exportPyCbk, NULL));
  tools->addWidget(new Button("export bmp", "Save this heightmap as a bitmap in ./hm.bmp", exportBmpCbk, NULL));

  vbox->addWidget(tools);

  // params box
  params = new ToolBar(0, 0, "Params", "Parameters of the current tool");
  vbox->addWidget(params);
  params->setVisible(false);

  // history
  history = new ToolBar(0, tools->y + 1 + tools->h, 15, "History", "History of operations");
  vbox->addWidget(history);
}

int main(int argc, char* argv[]) {
  TCODConsole::initRoot(HM_WIDTH, HM_HEIGHT, "height map tool", false, TCOD_RENDERER_OPENGL2);
  guicon = new TCODConsole(HM_WIDTH, HM_HEIGHT);
  guicon->setKeyColor(TCODColor(255, 0, 255));
  Widget::setConsole(guicon);
  TCODSystem::setFps(25);
  initColors();
  buildGui();
  hm = new TCODHeightMap(HM_WIDTH, HM_HEIGHT);
  hm_old = new TCODHeightMap(HM_WIDTH, HM_HEIGHT);
  rnd = new TCODRandom(seed);
  noise = new TCODNoise(2, rnd);
  uint8_t fade = 50;
  bool creditsEnd = false;

  while (!TCODConsole::isWindowClosed()) {
    render();
    guicon->setDefaultBackground(TCODColor(255, 0, 255));
    guicon->clear();
    Widget::renderWidgets();
    if (!creditsEnd) {
      creditsEnd = TCODConsole::renderCredits(HM_WIDTH - 20, HM_HEIGHT - 7, true);
    }
    if (Widget::focus) {
      if (fade < 200) fade += 20;
    } else {
      if (fade > 80) fade -= 20;
    }
    TCODConsole::blit(guicon, 0, 0, HM_WIDTH, HM_HEIGHT, TCODConsole::root, 0, 0, fade / 255.0f, fade / 255.0f);
    TCODConsole::flush();
    TCOD_key_t key;
    TCOD_mouse_t mouse;
    TCODSystem::checkForEvent(TCOD_EVENT_KEY_PRESS | TCOD_EVENT_MOUSE, &key, &mouse);
    Widget::updateWidgets(key, mouse);
    switch (key.c) {
      case '+':
        (new AddLevelOperation((mapmax - mapmin) / 50))->run();
        break;
      case '-':
        (new AddLevelOperation(-(mapmax - mapmin) / 50))->run();
        break;
      default:
        break;
    }
    switch (key.vk) {
      case TCODK_PRINTSCREEN:
        TCODSystem::saveScreenshot(NULL);
        break;
      default:
        break;
    }
  }

  return 0;
}
