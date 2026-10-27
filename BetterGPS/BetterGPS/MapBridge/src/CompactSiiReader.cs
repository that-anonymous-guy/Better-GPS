using System.Collections;
using System.Text;
using TruckLib;
using TruckLib.Sii;
namespace BetterGPSNavigation;

// Keep TruckLib's grammar, include resolution and value types authoritative.
// Long quoted attribute values are opaque to that grammar. Temporarily shorten
// them to avoid its per-character parser allocations, then restore exact strings.
// Unsupported encodings/quoting take the unchanged parser path.
internal static class CompactSiiReader
{
    private sealed class Unsupported:Exception;
    internal static SiiFile Open(string path,IFileSystem fs)
    {
        using var timing=PerformanceTrace.DetailObserver is null?null:PerformanceTrace.Detail("sii:"+path);
        try{
            var compact=new CompactFiles(fs);var parsed=SiiFile.Open(path,compact);
            foreach(var unit in parsed.Units)foreach(var key in unit.Attributes.Keys.ToArray())unit.Attributes[key]=compact.Restore(unit.Attributes[key]);
            return parsed;
        }catch(Unsupported){return SiiFile.Open(path,fs);}
    }
    private sealed class CompactFiles(IFileSystem inner):IFileSystem
    {
        private const char Marker='\uE000';
        private readonly Dictionary<string,string> originals=[];
        public char DirectorySeparator=>inner.DirectorySeparator;
        public bool FileExists(string path)=>inner.FileExists(path);
        public bool DirectoryExists(string path)=>inner.DirectoryExists(path);
        public IList<string> GetFiles(string path)=>inner.GetFiles(path);
        public string? GetParent(string path)=>inner.GetParent(path);
        public Stream Open(string path)=>new MemoryStream(ReadAllBytes(path),false);
        public byte[] ReadAllBytes(string path)=>Encoding.UTF8.GetBytes(ReadAllText(path));
        public string ReadAllText(string path)=>ReadAllText(path,Encoding.UTF8);
        public string ReadAllText(string path,Encoding encoding)
        {
            var bytes=inner.ReadAllBytes(path);
            if(bytes.Length>=4&&(bytes.AsSpan(0,4).SequenceEqual("ScsC"u8)||bytes.AsSpan(0,3).SequenceEqual("3nK"u8)||bytes.AsSpan(0,4).SequenceEqual("BSII"u8)))throw new Unsupported();
            var text=encoding.GetString(bytes);if(text.Contains(Marker)||text.Contains('\uFFFD')||text.Contains('\0'))throw new Unsupported();
            int first=0;SkipTrivia(text,ref first);
            int attributeDepth=text.AsSpan(first).StartsWith("SiiNunit",StringComparison.Ordinal)?2:1;
            int depth=0,last=0;var output=new StringBuilder(text.Length);
            for(int i=0;i<text.Length;){
                int before=i;SkipTrivia(text,ref i);if(i>=text.Length)break;if(i!=before)continue;
                char ch=text[i];if(ch=='{'){depth++;i++;continue;}if(ch=='}'){depth--;i++;continue;}
                if(ch!='"'){i++;continue;}
                int quote=i++,end=text.IndexOf('"',i);if(end<0)throw new Unsupported();
                var value=text.AsSpan(i,end-i);if(value.Contains('\r')||value.Contains('\n')||(end>i&&text[end-1]=='\\'))throw new Unsupported();
                int preceding=quote-1;while(preceding>=0&&char.IsWhiteSpace(text[preceding]))preceding--;
                if(depth>=attributeDepth&&preceding>=0&&text[preceding]==':'&&value.Length>12&&!char.IsWhiteSpace(value[0])&&!char.IsWhiteSpace(value[^1])){
                    string token=Marker+originals.Count.ToString("X");originals[token]=value.ToString();
                    output.Append(text,last,quote+1-last);output.Append(token);last=end;
                }
                i=end+1;
            }
            if(last==0)return text;output.Append(text,last,text.Length-last);return output.ToString();
        }
        internal object Restore(object value)
        {
            if(value is string text)return originals.TryGetValue(text,out var original)?original:text;
            if(value is IList list)for(int i=0;i<list.Count;i++)if(list[i] is {} item)list[i]=Restore(item);
            return value;
        }
        private static void SkipTrivia(string text,ref int i)
        {
            while(i<text.Length){
                if(char.IsWhiteSpace(text[i])||text[i]=='\uFEFF'){i++;continue;}
                if(text[i]=='#'||i+1<text.Length&&text[i]=='/'&&text[i+1]=='/'){while(i<text.Length&&text[i]!='\n')i++;continue;}
                if(i+1<text.Length&&text[i]=='/'&&text[i+1]=='*'){var end=text.IndexOf("*/",i+2,StringComparison.Ordinal);if(end<0)throw new Unsupported();i=end+2;continue;}
                break;
            }
        }
    }
}
