# UnrealWidgetGenCode

UnrealEngineでUMGの最適化の為に実装をC++で実装を行う必要に迫られる事があります。  
しかし、C++側からUMGのUIパーツやアニメーションにはアクセスする事が出来ません。  
C++からリフレクションを使って取得するか、Blueprint側から設定処理を実装するなどの手間がかかります。  
コード生成によって実装の手間を省く事が本プラグインの目標になります。

# 生成例

![UMGDesigner](https://raw.githubusercontent.com/laycnc/UnrealWidgetGenCode/documents/Documents/Image/UMGDesigner.png)

![BlueprintDesigner](https://raw.githubusercontent.com/laycnc/UnrealWidgetGenCode/documents/Documents/Image/BlueprintDesigner.png)


## UMGのパーツ

階層に登録されている＋IsVariableにチェックが入っている状態で変数として公開されている物が変数として公開されます。

## アニメーション

アニメーションタブに用意されている物はすべて変数として公開されます。


## C++側で生成されるコード

生成されるコードは2種類存在します。  
メンバ変数が定義されているだけの変数が定義されているBaseクラスと、ロジック実装用のImplクラスが生成されます。   
再生成によりロジックコードが消去される為、再生成によって消されても構わないBaseクラスとImplクラスの2種類に分割しています。  

### 変数定義用のBaseクラスのサンプル

下記が変数定義しているクラスが生成されます。  
Initialize関数でTWeakObjectPtrの変数が初期化されます。  
UPROPERTYで記述しないのはUMG側の変数名と被る為です。  

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

### ロジック実装用のImplクラスのサンプル

ロジック定義用のコードです。  
此方にロジックコードを実装してください。

```cpp
UCLASS(Blueprintable, meta = (WidgetGenImpl, WidgetGen = "/Game/WBP_Test"))
class WIDGETGENCODE_API UWidgetGenImplWBP_Test : public UWidgetGenBaseWBP_Test
{
	GENERATED_UCLASS_BODY()

public:	

};
```