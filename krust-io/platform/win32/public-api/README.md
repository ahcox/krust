Krust::IO components specific to the Win32 API
==============================================

Classes like `WindowPlatform` and `ApplicationPlatform` are intended to
nestle inside their platform-neutral equivalents.
This is similar to deriving platform-specific classes from the platform
neutral ones and using the template pattern but allows us to bake-in the
platform at compile time, avoiding dynamic dispatch.
