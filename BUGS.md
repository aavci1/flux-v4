# BUGS

# GENERAL

- [ ] When a window is closed the chrome disappears first window contents disappear later. ıt should fade out as a whole.
- [ ] Minimized apps should go to the dock, with a preview. User should be able to restore them. 
- [ ] Resizing the terminal app causes the content to be stretched, even if it is minimal. The framebuffer should be drawn without any scaling, just align to the top left unscaled.
- [x] When there is a window under another window. Window controls of the window underneath is highlighted when the mouse is over them despite the window in between. I noticed this when using the titlebars were aligned because of transparency. I'm not sure if this happens when the controls of a window is under the content area of another window.
- [x] The "ring" around the windows should not be counted towards the window content area. If the app requests a 800x800 window, the content area should 800x800 and the titlebar, the ring etc should be added on top.
- [x] Don't render the ring under the titlebar, just other 3 sides.
- [x] Title and window controls are not visible when the titlebar is over a white-ish background because of lack of contrast. Should add either a "shadow" to the title or the contorls or find a more elegant way.
- [ ] If an image is clicked in the finder. The image opens ia firefox, probably due to the mime/type assignment etc. Ideally it should open in the Preview app. The other problem is that as long as the firefox window is open in this case the files app can't be moved, closed etc. It doesn't receive the events.

# File Open/Save Dialogs
- [x] Dialogs can't be closed. Clicking the close button doesn't do anything. The dialog can't moved after clicking the close button.
- [x] Dialogs can't be closed with Esc key either. After pressing Esc windows still can be moved different from the close button.
- [x] In the off-chance the dialog is closed, trying to re-open it crashes editor app

# Shelll
- [ ] The dot under the running apps is not removed when the window is closed until user clicks the dock. Empty space or another app. 


Editor app crash backtrace:

Thread 1 "lambda-editor" received signal SIGSEGV, Segmentation fault.
0x000055555588a550 in ?? ()
(gdb) bt
#0  0x000055555588a550 in ?? ()
#1  0x00007ffff7d032d9 in std::function<void(std::any const&)>::operator() (this=0x555555873af0,
    __args#0=std::any containing lambda::InputEvent = {...})
    at /usr/include/c++/16.1.1/bits/std_function.h:581
#2  operator()<lambda::InputEvent> (__closure=<optimized out>, ev=...)
    at /home/aavci/flux-v4/src/UI/EventQueue.cpp:130
#3  std::__invoke_impl<void, lambda::detail::EventQueueImplAccess::dispatchOne(lambda::EventQueue&, lambda
::Event&)::<lambda(auto:45&)>, lambda::InputEvent&> (__f=...) at /usr/include/c++/16.1.1/bits/invoke.h:63
#4  std::__invoke<lambda::detail::EventQueueImplAccess::dispatchOne(lambda::EventQueue&, lambda::Event&)::
<lambda(auto:45&)>, lambda::InputEvent&> (__fn=...) at /usr/include/c++/16.1.1/bits/invoke.h:98
#5  std::__detail::__variant::__gen_vtable_impl<std::__detail::__variant::_Multi_array<std::__detail::__va
riant::__deduce_visit_result<void> (*)(lambda::detail::EventQueueImplAccess::dispatchOne(lambda::EventQueu
e&, lambda::Event&)::<lambda(auto:45&)>&&, std::variant<lambda::WindowLifecycleEvent, lambda::WindowEvent,
 lambda::InputEvent, lambda::TimerEvent, lambda::FrameEvent, lambda::CustomEvent>&)>, std::integer_sequenc
e<long unsigned int, 2> >::__visit_invoke (__visitor=..., __vars#0=std::variant [index 0] = {...})
    at /usr/include/c++/16.1.1/variant:1071
#6  std::__do_visit<std::__detail::__variant::__deduce_visit_result<void>, lambda::detail::EventQueueImplA
ccess::dispatchOne(lambda::EventQueue&, lambda::Event&)::<lambda(auto:45&)>, std::variant<lambda::WindowLi
fecycleEvent, lambda::WindowEvent, lambda::InputEvent, lambda::TimerEvent, lambda::FrameEvent, lambda::Cus
tomEvent>&> (__visitor=...) at /usr/include/c++/16.1.1/variant:1921
#7  std::visit<lambda::detail::EventQueueImplAccess::dispatchOne(lambda::EventQueue&, lambda::Event&)::<la
mbda(auto:45&)>, std::variant<lambda::WindowLifecycleEvent, lambda::WindowEvent, lambda::InputEvent, lambd
a::TimerEvent, lambda::FrameEvent, lambda::CustomEvent>&> (__visitor=...)
    at /usr/include/c++/16.1.1/variant:1982
#8  lambda::detail::EventQueueImplAccess::dispatchOne (q=..., event=std::variant [index 2] = {...})
    at /home/aavci/flux-v4/src/UI/EventQueue.cpp:123
#9  0x00007ffff7d036df in lambda::EventQueue::dispatch (this=0x5555555b5c30)
    at /home/aavci/flux-v4/src/UI/EventQueue.cpp:86
--Type <RET> for more, q to quit, c to continue without paging--
#10 0x00007ffff7e7aa10 in lambda::Application::exec (this=this@entry=0x7fffffffe530) at /usr/include/c++/16.1.1/bits/unique_ptr.h:192
#11 0x000055555556a5b0 in main (argc=<optimized out>, argv=<optimized out>) at /home/aavci/flux-v4/apps/lambda-editor/main.cpp:352
(gdb) q
A debugging session is active.

        Inferior 1 [process 170664] will be killed.

Quit anyway? (y or n) y
