#if !NO_UI_WEB_BROWSER
namespace Internal.Xilium.CefGlue
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.Runtime.InteropServices;
    using Internal.Xilium.CefGlue.Interop;
    
    /// <summary>
    /// Generic callback interface used for asynchronous completion.
    /// </summary>
    public abstract unsafe partial class CefCompletionCallback
    {
        private void on_complete(cef_completion_callback_t* self)
        {
            CheckSelf(self);

            OnComplete();
        }
        
        /// <summary>
        /// Method that will be called once the task is complete.
        /// </summary>
        protected abstract void OnComplete();
    }
}

#endif