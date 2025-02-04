#if !NO_LITE_DB
using System;
using static Internal.LiteDB.Constants;

namespace Internal.LiteDB
{
    /// <summary>
    /// Indicate that field are not persisted inside this document but it's a reference for another document (DbRef)
    /// </summary>
    public class BsonRefAttribute : Attribute
    {
        public string Collection { get; set; }

        public BsonRefAttribute(string collection)
        {
            this.Collection = collection;
        }

        public BsonRefAttribute()
        {
            this.Collection = null;
        }
    }
}
#endif