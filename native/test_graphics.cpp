/* Host resource/graphics smoke using the actual original game DAT. */
#include "graphics_host_shim.h"
#include "engine/pb.h"
#include "engine/partman.h"
#include "engine/GroupData.h"
#include "engine/gdrv.h"
#include "engine/fullscrn.h"

bool pb::FullTiltMode=false;
DatFile* pb::record_table=nullptr;
std::string pb::BasePath;
int fullscrn::GetResolution() { return 0; }
const resolution_info fullscrn::resolution_array[3]={
    {640,480,600,416,501},{800,600,752,520,502},{1024,768,960,666,503}
};
std::string pb::make_path_name(const std::string& file) { return BasePath+"/"+file; }

int main(int argc,char**argv) {
    pb::BasePath=argc>1?argv[1]:"pinball-9588/original-data";
    auto path=pb::make_path_name("PINBALL.DAT");
    pb::record_table=partman::load_records(path.c_str(),false);
    assert(pb::record_table);
    auto background=pb::record_table->GetBitmap(pb::record_table->record_labeled("background"));
    assert(background&&background->Width>200&&background->Height>200);
    auto palette=reinterpret_cast<ColorRgba*>(pb::record_table->field_labeled("background",FieldTypes::Palette));
    assert(palette);
    gdrv::display_palette(palette);
    auto table=pb::record_table->GetBitmap(pb::record_table->record_labeled("table"));
    assert(table&&table->Width>200&&table->Height>200);
    auto fontGroup=pb::record_table->record_labeled("pbmsg_ft");
    assert(fontGroup>=0);
    assert(pb::record_table->GetBitmap(fontGroup+33)->Width>0);
    gdrv_bitmap8 dest(8,4,false), source(8,4,false);
    gdrv::fill_bitmap(&dest,8,4,0,0,ColorRgba::Red());
    gdrv::fill_bitmap(&source,8,4,0,0,ColorRgba::Blue());
    source.BmpBufPtr1[3].Color=0;
    gdrv::copy_bitmap_w_transparency(&dest,8,4,0,0,&source,0,0);
    assert(dest.BmpBufPtr1[3].Color==ColorRgba::Red().Color);
    assert(dest.BmpBufPtr1[2].Color==ColorRgba::Blue().Color);
    std::printf("original DAT: %zu groups, sidebar %dx%d, table %dx%d at %d,%d, original font group %d; palette/transparency passed\n",
                pb::record_table->Groups.size(),background->Width,background->Height,
                table->Width,table->Height,table->XPosition,table->YPosition,fontGroup);
    if(argc>2) {
        auto file=std::fopen(argv[2],"wb");assert(file);
        std::fprintf(file,"P6\n%d %d\n255\n",table->Width,table->Height);
        for(int y=0;y<table->Height;++y)for(int x=0;x<table->Width;++x) {
            auto color=table->BmpBufPtr1[y*table->Stride+x];
            std::fputc(color.GetRed(),file);std::fputc(color.GetGreen(),file);std::fputc(color.GetBlue(),file);
        }
        std::fclose(file);
    }
    delete pb::record_table;pb::record_table=nullptr;
    return 0;
}
