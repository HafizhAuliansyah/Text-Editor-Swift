
using namespace std;

void copy(const char * t){
	OpenClipboard(0);
	EmptyClipboard();
	const size_t len = strlen(t) + 1;
	HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, len);
	memcpy(GlobalLock(h), t, len);
	GlobalUnlock(h);
	SetClipboardData(CF_TEXT, h);
	CloseClipboard();
}
void paste(){
	OpenClipboard(0);
	HANDLE len = GetClipboard(CF_TEXT);
	cout << (char*)len ;
	CloseClipboard;
}
