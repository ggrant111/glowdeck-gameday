"""Protect independent, continuous news ticker and bounded LCD photo loading."""
from pathlib import Path

main=Path("src/main.cpp").read_text()
photo=Path("include/NhlNewsPhoto.h").read_text()
news=Path("include/NhlNewsService.h").read_text()
config=Path("platformio.ini").read_text()

advance=main.split('void advanceNews(int direction) {',1)[1].split('void rebuildNewsTicker(){',1)[0]
card=main.split('void drawNewsCard() {',1)[1].split('void draw() {',1)[0]
ticker=main.split('void drawNewsTicker() {',1)[1].split('void tickNewsPhoto(uint32_t now)',1)[0]
assert 'newsTickerChar=0' not in advance
assert 'newsTickerPixel=0' not in advance
assert 'newsTickerChar=0' not in card
assert 'newsTickerPixel=0' not in card
assert 'newsTickerOffset' not in main
assert 'newsSlide' not in ticker, 'News ticker may not select the next card'
assert 'newsTickerWindow(w)' in ticker
assert 'newsGlyph(' in main and 'newsNextByte(' in main
assert 'newsTickerChar=newsNextByte(newsTickerChar);' in main
assert 'newsTickerText=text;' in main
assert 'rebuildNewsTicker();' in main
assert 'if(s3NewsTickerFrame)tft.presentRegion(0,286,480,24);' in main

# Ensure both platforms render from one bounded, optional JPEG cache.
assert 'NhlNewsPhoto::Cache newsPhotos;' in main
assert 'newsPhotos.begin(goalGif.mounted());' in main
assert 'newsPhotos.begin();' in main
assert 'NhlNewsPhoto::kSlots' not in main
assert 'newsPhotos.draw(story->image,px,py,pw,ph,' in card
assert 'bool newsJpegBlock(' in main
assert 'tft.pushImage(x,y,w,h,pixels);' in main
assert 'tft.drawPixel(xx,yy,pixels[row*w+col]);' in main
assert 'bool tickForgeNews' not in card
assert 'newsPhotos.fetch(story->image,now)' in main
assert 'if (!overlayController.active())tickNewsPhoto(millis());' in main
tick=main.split('void tickNewsPhoto(uint32_t now) {',1)[1].split('void drawNewsCard() {',1)[0]
for policy in ('newsViewEnabled()', 'overlayController.active()',
               'rearEvent!=RearEvent::IDLE','wledRestoreWouldConflict(now,5000UL)',
               'WiFi.status()!=WL_CONNECTED'):
    assert policy in tick,policy
assert 'kMaxBytes=65000UL' in photo and 'kMaxBytes=110000UL' in photo
assert photo.count('kSlots=10;')==2
assert 'http.collectHeaders(headers,1);' in photo
assert 'type.startsWith("image/jpeg")' in photo
assert 'total+uint32_t(got)>kMaxBytes' in photo
assert 'TJpgDec.getFsJpgSize' in photo
assert 'TJpgDec.drawFsJpg' in photo
assert 'url.startsWith("https://")' in photo
assert 'image/jpeg' in photo
assert config.count('bodmer/TJpg_Decoder@^1.1.0')==2
assert 'sample["images"].createNestedObject()["url"]=true;' in news
assert 'NhlNews::copy(story.image,sizeof(story.image),url);' in news
print("PASS news: continuous ticker, cropped JPEG thumbnails, safe idle fetch, CYD/S3 parity")
