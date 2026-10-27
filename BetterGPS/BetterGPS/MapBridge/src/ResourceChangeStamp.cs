using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
namespace BetterGPSNavigation;
internal static class ResourceChangeStamp
{
    [StructLayout(LayoutKind.Sequential)] private struct BasicInfo
    {public long Creation,Access,Write,Change;public uint Attributes;}
    [DllImport("kernel32.dll",SetLastError=true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetFileInformationByHandleEx(SafeFileHandle handle,int informationClass,out BasicInfo info,uint size);
    internal static bool TryRead(SafeFileHandle handle,out long change)
    {
        change=0;
        if(!OperatingSystem.IsWindows())return false;
        if(!GetFileInformationByHandleEx(handle,0,out var info,(uint)Marshal.SizeOf<BasicInfo>())||info.Change<=0)return false;
        change=info.Change;return true;
    }
}
