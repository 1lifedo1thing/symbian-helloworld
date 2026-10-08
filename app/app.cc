// Hello World for Symbian S60 3rd Edition (Symbian OS 9.x, EKA2, ARMv5T).
//
// The platform SDK provides the process entry point, so this program starts
// at an ordinary main(). It opens a single Window Server window, draws
// "Hello, World!" in the middle of the screen, and leaves when the user picks
// Exit (right softkey or Escape) or closes the window from the task list.
//
// The Window Server owns every window on a Symbian device and delivers both
// redraw requests and input events. The sequence below is the documented W32
// pattern:
//
//   connect to the server -> window group -> window
//   -> wait for redraw/input events -> close everything again

#include <e32std.h>
#include <w32std.h>

namespace {

// Handles are application-chosen identifiers. Redraw and input events that
// belong to our window are reported back with the same window handle.
constexpr TUint32 kWindowGroupHandle = 1;
constexpr TUint32 kWindowHandle = 2;

constexpr char kGreeting[] = "Hello, World!";
constexpr char kHint[] = "Press Exit to close.";
constexpr TInt kGreetingLength = static_cast<TInt>(sizeof(kGreeting) - 1);
constexpr TInt kHintLength = static_cast<TInt>(sizeof(kHint) - 1);

// The drawing APIs take UTF-16 descriptors. Both literals above are plain
// ASCII, so widening one byte into one UTF-16 code unit is exact. Replace
// this adapter with a real UTF-8 decoder before shipping translated text.
constexpr TInt kMaxTextUnits = 64;

TInt Widen(const char* text, TInt length, TUint16* units, TInt capacity) {
  TInt count = 0;
  while (count < length && count < capacity) {
    units[count] =
        static_cast<TUint16>(static_cast<unsigned char>(text[count]));
    ++count;
  }
  return count;
}

// Draws one ASCII line horizontally centred on center_x, with its top at y.
void DrawLine(CWindowGc* gc, CFont* font, const char* text, TInt length,
              TInt center_x, TInt y) {
  TUint16 units[kMaxTextUnits];
  const TInt count = Widen(text, length, units, kMaxTextUnits);
  const TPtrC16 line(units, count);
  gc->DrawText(line, TPoint(center_x - font->TextWidthInPixels(line) / 2, y));
}

void Draw(CWindowGc* gc, CFont* font, TSize size) {
  // BeginRedraw() only names the region that needs painting, and this window
  // is cheap to repaint, so the whole surface is filled on every redraw.
  gc->SetPenStyle(CGraphicsContext::ENullPen);
  gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
  gc->SetBrushColor(TRgb(0x00101820));
  gc->DrawRect(TRect(TPoint(0, 0), size));

  // A font must be selected before DrawText() and TextWidthInPixels().
  gc->UseFont(font);
  gc->SetPenStyle(CGraphicsContext::ESolidPen);
  gc->SetPenColor(TRgb(0x00ffffff));

  const TInt center_x = size.iWidth / 2;
  const TInt middle = size.iHeight / 2;
  DrawLine(gc, font, kGreeting, kGreetingLength, center_x, middle - 16);

  gc->SetPenColor(TRgb(0x0080c0ff));
  DrawLine(gc, font, kHint, kHintLength, center_x, middle + 20);

  gc->DiscardFont();
}

TInt RunWindow(RWsSession* session, CWsScreenDevice* screen, CWindowGc* gc) {
  const TSize size = screen->SizeInPixels();
  if (size.iWidth < 176 || size.iHeight < 208) {
    return KErrNotSupported;  // Smaller than any supported S60 screen.
  }

  // Ask the server for its closest default font instead of naming a vendor
  // typeface, which would not exist on every handset.
  CFont* font = nullptr;
  _LIT(KTypeface, "");
  const TFontSpec specification(KTypeface, size.iHeight >= 480 ? 180 : 120);
  TInt result =
      screen->GetNearestFontToDesignHeightInTwips(font, specification);
  if (result != KErrNone) {
    return result;
  }

  RWindowGroup group(*session);
  result = group.Construct(kWindowGroupHandle, ETrue);
  if (result != KErrNone) {
    screen->ReleaseFont(font);
    return result;
  }

  RWindow window(*session);
  result = window.Construct(group, kWindowHandle);
  if (result != KErrNone) {
    group.Close();
    screen->ReleaseFont(font);
    return result;
  }

  group.SetOrdinalPosition(0);
  window.SetExtent(TPoint(0, 0), size);
  window.SetVisible(ETrue);
  window.Activate();

  // One input request and one redraw request are outstanding at a time. Only
  // this thread ever waits on them.
  TRequestStatus events;
  TRequestStatus redraws;
  session->EventReady(&events);
  session->RedrawReady(&redraws);
  window.Invalidate();
  session->Flush();

  TBool running = ETrue;
  while (running) {
    // Park until the Window Server has something to report.
    User::WaitForRequest(events, redraws);

    if (events != KRequestPending) {
      if (events.Int() != KErrNone) {
        result = events.Int();
        break;
      }
      TWsEvent event;
      session->GetEvent(event);
      switch (event.Type()) {
        case EEventKey:
          // Right softkey ("Exit") and Escape both leave the application.
          if (event.Key()->iCode == EKeyEscape ||
              event.Key()->iCode == EKeyDevice1) {
            running = EFalse;
          }
          break;
        case EEventWindowClose:
          running = EFalse;
          break;
        case EEventFocusGained:
          window.Invalidate();  // Repaint after returning to the foreground.
          break;
        default:
          break;
      }
      if (running) {
        session->EventReady(&events);
      }
    }

    if (redraws != KRequestPending) {
      if (redraws.Int() != KErrNone) {
        result = redraws.Int();
        break;
      }
      TWsRedrawEvent redraw;
      session->GetRedraw(redraw);
      if (redraw.Handle() == kWindowHandle) {
        window.BeginRedraw(redraw.Rect());
        gc->Activate(window);
        Draw(gc, font, size);
        gc->Deactivate();
        window.EndRedraw();
      }
      if (running) {
        session->RedrawReady(&redraws);
      }
    }

    session->Flush();
  }

  // Cancel and drain whatever is still outstanding before closing the window,
  // the window group and the font.
  session->EventReadyCancel();
  session->RedrawReadyCancel();
  if (events == KRequestPending) {
    User::WaitForRequest(events);
  }
  if (redraws == KRequestPending) {
    User::WaitForRequest(redraws);
  }
  window.Close();
  group.Close();
  session->Flush();
  screen->ReleaseFont(font);
  return result;
}

}  // namespace

int main() {
  RWsSession session;
  TInt result = session.Connect();
  if (result != KErrNone) {
    return result;  // No Window Server: there is nothing to draw on.
  }
  {
    CWsScreenDevice screen(session);
    result = screen.Construct();
    if (result == KErrNone) {
      CWindowGc gc(&screen);
      result = gc.Construct();
      if (result == KErrNone) {
        result = RunWindow(&session, &screen, &gc);
      }
    }
  }
  session.Close();
  return result;
}
