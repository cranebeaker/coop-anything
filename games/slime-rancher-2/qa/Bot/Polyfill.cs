namespace System.Runtime.CompilerServices
{
    [AttributeUsage(AttributeTargets.All, Inherited = false)]
    internal sealed class NullableAttribute : Attribute { public NullableAttribute(byte b) { } public NullableAttribute(byte[] b) { } }
    [AttributeUsage(AttributeTargets.All, Inherited = false)]
    internal sealed class NullableContextAttribute : Attribute { public NullableContextAttribute(byte b) { } }
}
