#if !DEPLOY
// *****************************************************************************
// 
//  © Component Factory Pty Ltd 2012. All rights reserved.
//	The software and associated documentation supplied hereunder are the 
//  proprietary information of Component Factory Pty Ltd, 17/267 Nepean Hwy, 
//  Seaford, Vic 3198, Australia and are supplied subject to licence terms.
// 
//
// *****************************************************************************

using System;
using System.Text;
using System.Data;
using System.Drawing;
using System.Drawing.Design;
using System.ComponentModel;
using System.Windows.Forms;
using System.Diagnostics;
using Internal.ComponentFactory.Krypton.Toolkit;

namespace Internal.ComponentFactory.Krypton.Ribbon
{
    /// <summary>
    /// Delegate used for hooking into a KryptonRibbonContext typed collection.
    /// </summary>
    public delegate void RibbonContextHandler(object sender, TypedCollectionEventArgs<KryptonRibbonContext> e);

    /// <summary>
    /// Specialise the generic collection with type specific rules for context item accessor.
    /// </summary>
    public class KryptonRibbonContextCollection : TypedCollection<KryptonRibbonContext>
    {
        #region Public
        /// <summary>
        /// Gets the item with the provided unique name.
        /// </summary>
        /// <param name="name">Name of the ribbon context instance.</param>
        /// <returns>Item at specified index.</returns>
        public override KryptonRibbonContext this[string name]
        {
            get
            {
                // Search for a context with the same name as that requested.
                foreach (KryptonRibbonContext context in this)
                    if (context.ContextName == name)
                        return context;

                // Let base class perform standard processing
                return base[name];
            }
        }
        #endregion
    }
}

#endif