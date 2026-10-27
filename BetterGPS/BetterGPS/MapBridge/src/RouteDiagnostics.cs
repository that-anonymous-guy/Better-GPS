using System.Text;
namespace BetterGPSNavigation;
internal static class RouteDiagnostics
{
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string,byte> warned=new();
    internal static void Try(Action write)
    {
        try{write();}
        catch(Exception e) when(e is IOException or UnauthorizedAccessException)
        {if(warned.TryAdd(e.GetType().Name,0))Console.Error.WriteLine("Journal GPS indisponible ; guidage maintenu : "+e.Message);}
    }
    internal static string? CompactOutput;
    private static bool Skip(string path)=>CompactOutput is {} root&&Path.GetFullPath(path).StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase);
    internal static void WriteText(string path,string contents,Encoding? encoding=null){if(!Skip(path))Try(()=>File.WriteAllText(path,contents,encoding??new UTF8Encoding(false)));}
    internal static void WriteBytes(string path,byte[] contents){if(!Skip(path))Try(()=>File.WriteAllBytes(path,contents));}
    internal static void WriteText(string path,Func<string> contents,Encoding? encoding=null){if(!Skip(path))Try(()=>File.WriteAllText(path,contents(),encoding??new UTF8Encoding(false)));}
    internal static void WriteBytes(string path,Func<byte[]> contents){if(!Skip(path))Try(()=>File.WriteAllBytes(path,contents()));}
}
