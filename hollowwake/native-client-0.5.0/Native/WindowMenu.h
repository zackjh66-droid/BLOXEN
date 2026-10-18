// System-command state for the single virtual fullscreen window. No popup or
// desktop menu renderer is advertised. Game/Slate lobby menus are separate.
struct SystemItem {UINT command,state;};
static SystemItem systemItems[]={{0xf120,1},{0xf010,1},{0xf000,1},{0xf020,1},{0xf030,1},{0xf060,0}};
static int menuIdentity;
static void ResetSystemMenu(){for(auto& item:systemItems)item.state=item.command==0xf060?0:1;}
static HMENU WINAPI SystemMenu(HWND h,BOOL reset){std::lock_guard<std::recursive_mutex> l(mutex);if(!Valid(h)){SetLastError(ERROR_INVALID_WINDOW_HANDLE);return nullptr;}if(reset){ResetSystemMenu();return nullptr;}if(!(window->style&0x80000))return nullptr;return reinterpret_cast<HMENU>(&menuIdentity);}
static SystemItem* MenuItem(HMENU menu,UINT item,UINT flags){if(menu!=reinterpret_cast<HMENU>(&menuIdentity)||!window||!window->alive||!(window->style&0x80000)){SetLastError(ERROR_INVALID_MENU_HANDLE);return nullptr;}
 if(flags&0x400){if(item<sizeof(systemItems)/sizeof(systemItems[0]))return &systemItems[item];}else for(auto& entry:systemItems)if(entry.command==item)return &entry;SetLastError(ERROR_MENU_ITEM_NOT_FOUND);return nullptr;
}
static UINT WINAPI EnableSystemItem(HMENU menu,UINT item,UINT flags){std::lock_guard<std::recursive_mutex> l(mutex);if((flags&~0x403)||(flags&3)==3){SetLastError(ERROR_INVALID_FLAGS);return (UINT)-1;}auto entry=MenuItem(menu,item,flags);if(!entry)return (UINT)-1;auto previous=entry->state;entry->state=flags&3;return previous;}
static UINT WINAPI SystemItemState(HMENU menu,UINT item,UINT flags){std::lock_guard<std::recursive_mutex> l(mutex);if(flags&~0x400){SetLastError(ERROR_INVALID_FLAGS);return (UINT)-1;}auto entry=MenuItem(menu,item,flags);return entry?entry->state:(UINT)-1;}
