DeepL translation generated from Readme.jp　

[日本語](https://github.com/laycnc/UnrealWidgetGenCode/blob/main/README_jp.md)

# UnrealWidgetGenCode

UnrealEngine sometimes needs to be implemented in C++ for UMG optimization.  
However, UMG UI parts and animations cannot be accessed from the C++ side.  
It takes time and effort to retrieve them from C++ using reflection, or to implement the setting process from the Blueprint side.  
The goal of this plugin is to eliminate this implementation effort by generating code.

# Example of code generation

![UMGDesigner](https://raw.githubusercontent.com/laycnc/UnrealWidgetGenCode/documents/Documents/Image/UMGDesigner.png)

![BlueprintDesigner](https://raw.githubusercontent.com/laycnc/UnrealWidgetGenCode/documents/Documents/Image/BlueprintDesigner.png)

## Parts of UMG

Things that are registered in the hierarchy + things that are published as variables with IsVariable checked will be published as variables.

## Animation

All the items prepared in the Animation tab will be published as variables.


## Code generated on the C++ side

There are two types of generated code.  
A Base class in which only member variables are defined and an Impl class for logic implementation are generated.   
Since the logic code is deleted by re-generating, the code is divided into two types: the Base class and the Impl class, which may be deleted by re-generating.  

### Sample of Base class for defining variables

The following will generate a class with variable definitions.  
The variables of TWeakObjectPtr are initialized by the Initialize function.  
The reason why it is not written in UPROPERTY is because the variable names on the UMG side are covered.  


```cpp
UCLASS(meta = (WidgetGenBase, WidgetGen = "/Game/WBP_Test"))
class WIDGETGENCODE_API UWidgetGenBaseWBP_Test : public UMyUserWidget
{
	GENERATED_UCLASS_BODY()

public:	
	virtual bool Initialize() override;

public:	

	TWeakObjectPtr<UWidgetAnimation> NewAnimation_3;
	TWeakObjectPtr<UWidgetAnimation> NewAnimation_2;
	TWeakObjectPtr<UWidgetAnimation> NewAnimation;
	TWeakObjectPtr<UButton> Button_0;
	TWeakObjectPtr<UCheckBox> CheckBox_47;
	TWeakObjectPtr<UEditableTextBox> EditableTextBox_0;
	TWeakObjectPtr<UImage> Image_56;
	TWeakObjectPtr<USlider> Slider_24;
};
```

### Sample Impl class for logic implementation

This is the code for the logic definition.  
Implement your logic code here.

```cpp
UCLASS(Blueprintable, meta = (WidgetGenImpl, WidgetGen = "/Game/WBP_Test"))
class WIDGETGENCODE_API UWidgetGenImplWBP_Test : public UWidgetGenBaseWBP_Test
{
	GENERATED_UCLASS_BODY()

public:	

};
```