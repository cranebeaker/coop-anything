// Read-only assembly browser for reverse engineering (Mono.Cecil, the copy shipped with BepInEx).
//   tfdump <dll> types <regex>            type names matching
//   tfdump <dll> members <type>           fields, properties, methods with signatures
//   tfdump <dll> il <type> <method>       IL of every overload of a method
//   tfdump <dll> strings <regex>          string literals (ldstr) matching, with the method that uses them
//   tfdump <dll> refs <member-regex>      methods whose IL references a matching field or method
using System.Text.RegularExpressions;
using Mono.Cecil;
using Mono.Cecil.Cil;

if (args.Length < 3) { Console.Error.WriteLine("usage: tfdump <dll> types|members|il|strings|refs ..."); return 2; }
var resolver = new DefaultAssemblyResolver();
resolver.AddSearchDirectory(Path.GetDirectoryName(Path.GetFullPath(args[0]))!);
var asm = AssemblyDefinition.ReadAssembly(args[0], new ReaderParameters { AssemblyResolver = resolver });
var all = asm.MainModule.GetTypes().ToList();
TypeDefinition Find(string n) => all.FirstOrDefault(t => t.FullName == n) ?? all.First(t => t.Name == n);

switch (args[1])
{
    case "types":
        foreach (var t in all.Where(t => Regex.IsMatch(t.FullName, args[2])))
            Console.WriteLine($"{t.FullName} : {t.BaseType?.Name}");
        break;
    case "members":
    {
        var t = Find(args[2]);
        Console.WriteLine($"{t.FullName} : {t.BaseType?.FullName}");
        foreach (var f in t.Fields) Console.WriteLine($"  F {(f.IsStatic ? "static " : "")}{(f.IsPublic ? "public " : "")}{f.FieldType.Name} {f.Name}");
        foreach (var p in t.Properties) Console.WriteLine($"  P {p.PropertyType.Name} {p.Name}");
        foreach (var m in t.Methods)
            Console.WriteLine($"  M {(m.IsStatic ? "static " : "")}{(m.IsPublic ? "public " : "")}{m.ReturnType.Name} {m.Name}({string.Join(", ", m.Parameters.Select(p => p.ParameterType.Name + " " + p.Name))})");
        break;
    }
    case "il":
    {
        var t = Find(args[2]);
        foreach (var m in t.Methods.Where(m => m.Name == args[3] && m.HasBody))
        {
            Console.WriteLine($"== {m.FullName}");
            foreach (var i in m.Body.Instructions) Console.WriteLine("  " + i);
        }
        break;
    }
    case "strings":
        foreach (var t in all)
        foreach (var m in t.Methods.Where(m => m.HasBody))
        foreach (var i in m.Body.Instructions.Where(i => i.OpCode == OpCodes.Ldstr && Regex.IsMatch((string)i.Operand, args[2])))
            Console.WriteLine($"{t.FullName}::{m.Name}  \"{i.Operand}\"");
        break;
    case "refs":
        foreach (var t in all)
        foreach (var m in t.Methods.Where(m => m.HasBody))
        foreach (var i in m.Body.Instructions.Where(i => i.Operand is MemberReference r && Regex.IsMatch(r.DeclaringType?.Name + "::" + r.Name, args[2])))
            Console.WriteLine($"{t.FullName}::{m.Name}  {i.OpCode} {((MemberReference)i.Operand).FullName}");
        break;
}
return 0;
