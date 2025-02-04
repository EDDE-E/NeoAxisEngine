#if !NO_UI_WEB_BROWSER
//
// This file manually written from cef/include/internal/cef_types.h.
// C API name: cef_value_type_t.
//
namespace Internal.Xilium.CefGlue
{
    /// <summary>
    /// Supported value types.
    /// </summary>
    public enum CefValueType
    {
        Invalid = 0,
        Null,
        Bool,
        Int,
        Double,
        String,
        Binary,
        Dictionary,
        List,
    }
}

#endif