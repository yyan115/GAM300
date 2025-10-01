using System;
using System.Runtime.InteropServices;

namespace GameScripts
{
    public class ScriptTest
    {
        [UnmanagedCallersOnly(EntryPoint = "HelloFromCSharp")]
        public static int HelloFromCSharp(int value)
        {
            Console.WriteLine($"Hello from C#! Value: {value}");
            return value * 2;
        }
    }
}